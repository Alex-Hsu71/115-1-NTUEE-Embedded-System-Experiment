/*
 * HW7 - CMSIS-DSP FIR low-pass on LSM6DSL accelerometer data
 *
 *   TIM7 ISR (100 Hz) --thread flag--> SensorTask   (osPriorityHigh)
 *        SensorTask --rawQ (10-sample blocks)--> DspTask (osPriorityAboveNormal)
 *        DspTask    --outQ (raw + filtered)----> CommTask (osPriorityBelowNormal)
 *        CommTask sends the before/after data to the PC over WiFi (TCP client,
 *        see wifi_config.h). USART1 (ST-LINK VCP, 115200 8N1) carries status
 *        messages and the test run; sensor lines fall back to USART1 while
 *        WiFi is not connected.
 *
 * Start-up: CommTask connects WiFi first, then signals SensorTask to start
 * TIM7, so the boot-time test run is not lost while WiFi is connecting.
 *
 * Signal source:
 *   TEST   : known synthetic signal (runs at boot and on every USER button press)
 *              X = sin(2*pi*2*t) + 0.5*sin(2*pi*30*t)   -> 30 Hz must be removed
 *              Y = unit impulse  d[n]                     -> output = FIR coefficients
 *              Z = unit step     u[n]                     -> output settles to DC gain 1
 *            CMSIS-DSP output is also compared on-board against a plain-C
 *            double-precision FIR and the max error is reported.
 *   SENSOR : LSM6DSL accelerometer X/Y/Z in mg
 *
 * Line format (identical on UART and TCP):
 *   #TEST_BEGIN fs=100 fc=5 taps=31 samples=300
 *   T,n,x,y,z,fx,fy,fz
 *   #TEST_END samples=300 max_err=1.2e-07 cycles_per_block=1234
 *   S,n,x,y,z,fx,fy,fz
 */
#include "app.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "dsp_filter.h"
#include "stm32l475e_iot01_accelero.h"
#include "wifi.h"
#include "wifi_config.h"

#define FLAG_SAMPLE      0x0001U
#define FLAG_START       0x0002U       /* CommTask -> SensorTask: WiFi setup done */
#define WIFI_TIMEOUT_MS  1000U
#define RECONNECT_MS     3000U         /* retry TCP connection this often */
#define TX_BUF_SIZE      1024U         /* one block = 10 lines, < ES_WIFI_PAYLOAD_SIZE */
#define TEST_SAMPLES     300U          /* 3 s of test signal */
#define BUTTON_PIN       GPIO_PIN_13   /* blue USER button, PC13 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum { MODE_SENSOR = 0, MODE_TEST = 1 } SourceMode_t;

typedef struct
{
  uint32_t n0;                                 /* index of first sample in block */
  uint8_t  mode;
  float    x[DSP_NUM_AXES][DSP_BLOCK_SIZE];
} RawBlock_t;

typedef struct
{
  uint32_t n0;
  uint8_t  mode;
  uint8_t  test_begin;                         /* first block of a test run */
  uint8_t  test_end;                           /* last block of a test run  */
  float    raw[DSP_NUM_AXES][DSP_BLOCK_SIZE];
  float    filt[DSP_NUM_AXES][DSP_BLOCK_SIZE];
  float    max_err;                            /* valid when test_end */
  uint32_t cycles;                             /* DWT cycles for 3-axis FIR */
} OutBlock_t;

extern UART_HandleTypeDef huart1;
extern void Error_Handler(void);

static osThreadId_t sensorTaskHandle;
static osThreadId_t dspTaskHandle;
static osThreadId_t commTaskHandle;
static osMessageQueueId_t rawQ;
static osMessageQueueId_t outQ;
static osMutexId_t uartMutex;

static volatile uint8_t  test_request = 1U;    /* run the test once at start */
static volatile uint32_t raw_drops;
static volatile uint32_t out_drops;

static void SensorTask(void *argument);
static void DspTask(void *argument);
static void CommTask(void *argument);

/* ------------------------------------------------------------------------- */

static void uart_send(const char *s, int len)
{
  if (len > 0)
  {
    osMutexAcquire(uartMutex, osWaitForever);
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)len, HAL_MAX_DELAY);
    osMutexRelease(uartMutex);
  }
}

static void uart_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void App_Init(void)
{
  rawQ = osMessageQueueNew(4, sizeof(RawBlock_t), NULL);
  outQ = osMessageQueueNew(8, sizeof(OutBlock_t), NULL);
  uartMutex = osMutexNew(NULL);

  const osThreadAttr_t sensor_attr = {
    .name = "SensorTask", .stack_size = 1024, .priority = osPriorityHigh };
  const osThreadAttr_t dsp_attr = {
    .name = "DspTask", .stack_size = 2048, .priority = osPriorityAboveNormal };
  const osThreadAttr_t comm_attr = {
    .name = "CommTask", .stack_size = 4096, .priority = osPriorityBelowNormal };

  sensorTaskHandle = osThreadNew(SensorTask, NULL, &sensor_attr);
  dspTaskHandle    = osThreadNew(DspTask, NULL, &dsp_attr);
  commTaskHandle   = osThreadNew(CommTask, NULL, &comm_attr);

  if (!rawQ || !outQ || !uartMutex || !sensorTaskHandle || !dspTaskHandle || !commTaskHandle)
  {
    Error_Handler();
  }
}

void App_TimerCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM7 && sensorTaskHandle != NULL)
  {
    osThreadFlagsSet(sensorTaskHandle, FLAG_SAMPLE);
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BUTTON_PIN)
  {
    test_request = 1U;
  }
  else if (GPIO_Pin == GPIO_PIN_1)
  {
    SPI_WIFI_ISR();                            /* ISM43362 DRDY (PE1) */
  }
}

/* ------------------------------------------------------------------------- */

static void test_signal(uint32_t n, float out[DSP_NUM_AXES])
{
  const double t = (double)n / (double)FIR_FS_HZ;
  out[0] = (float)(sin(2.0 * M_PI * 2.0 * t) + 0.5 * sin(2.0 * M_PI * 30.0 * t));
  out[1] = (n == 0U) ? 1.0f : 0.0f;
  out[2] = 1.0f;
}

static void accel_init(void)
{
  if (BSP_ACCELERO_Init() != ACCELERO_OK)
  {
    static const char msg[] = "#ERROR accelerometer init failed\r\n";
    uart_send(msg, sizeof(msg) - 1);
    return;
  }
  /* BSP default ODR is 52 Hz; raise it to 104 Hz so every 100 Hz sample is fresh */
  uint8_t ctrl = SENSOR_IO_Read(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_CTRL1_XL);
  ctrl = (uint8_t)((ctrl & (uint8_t)~LSM6DSL_ODR_BITPOSITION) | LSM6DSL_ODR_104Hz);
  SENSOR_IO_Write(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_CTRL1_XL, ctrl);
}

static void SensorTask(void *argument)
{
  (void)argument;
  extern TIM_HandleTypeDef htim7;
  RawBlock_t blk;
  uint32_t idx = 0U;
  uint32_t n = 0U;
  SourceMode_t mode = MODE_SENSOR;

  accel_init();
  /* wait until CommTask has finished bringing up WiFi (success or not) */
  osThreadFlagsWait(FLAG_START, osFlagsWaitAny, osWaitForever);
  HAL_TIM_Base_Start_IT(&htim7);

  for (;;)
  {
    osThreadFlagsWait(FLAG_SAMPLE, osFlagsWaitAny, osWaitForever);

    if (idx == 0U && test_request)             /* switch only on block boundary */
    {
      test_request = 0U;
      mode = MODE_TEST;
      n = 0U;
    }

    float s[DSP_NUM_AXES];
    if (mode == MODE_TEST)
    {
      test_signal(n, s);
    }
    else
    {
      int16_t xyz[3];
      BSP_ACCELERO_AccGetXYZ(xyz);             /* mg */
      s[0] = (float)xyz[0];
      s[1] = (float)xyz[1];
      s[2] = (float)xyz[2];
    }
    for (uint32_t a = 0; a < DSP_NUM_AXES; a++)
    {
      blk.x[a][idx] = s[a];
    }
    idx++;
    n++;

    if (idx == DSP_BLOCK_SIZE)
    {
      blk.n0 = n - DSP_BLOCK_SIZE;
      blk.mode = (uint8_t)mode;
      if (osMessageQueuePut(rawQ, &blk, 0U, 0U) != osOK)
      {
        raw_drops++;
      }
      idx = 0U;
      if (mode == MODE_TEST && n >= TEST_SAMPLES)
      {
        mode = MODE_SENSOR;
        n = 0U;
      }
    }
  }
}

static void DspTask(void *argument)
{
  (void)argument;
  static RawBlock_t in;
  static OutBlock_t out;
  double ref[DSP_BLOCK_SIZE];
  float max_err = 0.0f;
  int last_mode = -1;

  /* DWT cycle counter for timing the filter */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  DSP_Filter_Reset();

  for (;;)
  {
    osMessageQueueGet(rawQ, &in, NULL, osWaitForever);

    if ((int)in.mode != last_mode || in.n0 == 0U)
    {
      DSP_Filter_Reset();                      /* start every run from zero state */
      max_err = 0.0f;
      last_mode = (int)in.mode;
    }

    uint32_t t0 = DWT->CYCCNT;
    for (uint32_t a = 0; a < DSP_NUM_AXES; a++)
    {
      DSP_Filter_Process(a, in.x[a], out.filt[a]);
    }
    out.cycles = DWT->CYCCNT - t0;

    if (in.mode == MODE_TEST)
    {
      for (uint32_t a = 0; a < DSP_NUM_AXES; a++)
      {
        DSP_Reference_Process(a, in.x[a], ref);
        for (uint32_t i = 0; i < DSP_BLOCK_SIZE; i++)
        {
          float e = fabsf(out.filt[a][i] - (float)ref[i]);
          if (e > max_err)
          {
            max_err = e;
          }
        }
      }
    }

    memcpy(out.raw, in.x, sizeof(out.raw));
    out.n0 = in.n0;
    out.mode = in.mode;
    out.test_begin = (uint8_t)(in.mode == MODE_TEST && in.n0 == 0U);
    out.test_end = (uint8_t)(in.mode == MODE_TEST && in.n0 + DSP_BLOCK_SIZE >= TEST_SAMPLES);
    out.max_err = max_err;

    if (osMessageQueuePut(outQ, &out, 0U, 0U) != osOK)
    {
      out_drops++;
    }
  }
}

/* ---------------------------- CommTask ----------------------------------- */

static void uart_printf(const char *fmt, ...)
{
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (len > (int)sizeof(buf) - 1)
  {
    len = (int)sizeof(buf) - 1;
  }
  uart_send(buf, len);
}

static uint8_t wifi_up;                        /* joined the access point */
static uint8_t tcp_up;                         /* TCP connection to the PC open */
static uint8_t remote_ip[4] = REMOTE_IP;

static void tcp_open(void)
{
  if (WIFI_OpenClientConnection(0, WIFI_TCP_PROTOCOL, "HW7", remote_ip,
                                REMOTE_PORT, 0) == WIFI_STATUS_OK)
  {
    tcp_up = 1U;
    uart_printf("#WIFI TCP connected to %d.%d.%d.%d:%d\r\n", remote_ip[0],
                remote_ip[1], remote_ip[2], remote_ip[3], REMOTE_PORT);
  }
  else
  {
    uart_printf("#WIFI TCP connect to %d.%d.%d.%d:%d failed (is live_plot.py --tcp running?)\r\n",
                remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3], REMOTE_PORT);
  }
}

static void wifi_bring_up(void)
{
  uint8_t ip[4];

  uart_printf("#WIFI init...\r\n");
  if (WIFI_Init() != WIFI_STATUS_OK)
  {
    uart_printf("#WIFI ERROR module init failed, using UART only\r\n");
    return;
  }
  uart_printf("#WIFI connecting to SSID \"%s\"...\r\n", WIFI_SSID);
  if (WIFI_Connect(WIFI_SSID, WIFI_PASSWORD, WIFI_ECN_WPA2_PSK) != WIFI_STATUS_OK)
  {
    uart_printf("#WIFI ERROR cannot join \"%s\" (2.4 GHz only), using UART only\r\n", WIFI_SSID);
    return;
  }
  wifi_up = 1U;
  if (WIFI_GetIP_Address(ip, sizeof(ip)) == WIFI_STATUS_OK)
  {
    uart_printf("#WIFI joined, board IP %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
  }
  tcp_open();
}

/* Format one output block as text lines; returns length. */
static int format_block(const OutBlock_t *b, char *buf, int size)
{
  int len = 0;

  if (b->test_begin)
  {
    len += snprintf(buf + len, size - len, "#TEST_BEGIN fs=%d fc=%d taps=%d samples=%u\r\n",
                    (int)FIR_FS_HZ, (int)FIR_FC_HZ, FIR_NUM_TAPS, (unsigned)TEST_SAMPLES);
  }
  for (uint32_t i = 0; i < DSP_BLOCK_SIZE && len < size; i++)
  {
    if (b->mode == MODE_TEST)
    {
      len += snprintf(buf + len, size - len, "T,%lu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\r\n",
                      (unsigned long)(b->n0 + i),
                      b->raw[0][i], b->raw[1][i], b->raw[2][i],
                      b->filt[0][i], b->filt[1][i], b->filt[2][i]);
    }
    else
    {
      len += snprintf(buf + len, size - len, "S,%lu,%.0f,%.0f,%.0f,%.2f,%.2f,%.2f\r\n",
                      (unsigned long)(b->n0 + i),
                      b->raw[0][i], b->raw[1][i], b->raw[2][i],
                      b->filt[0][i], b->filt[1][i], b->filt[2][i]);
    }
  }
  if (b->test_end && len < size)
  {
    len += snprintf(buf + len, size - len,
                    "#TEST_END samples=%u max_err=%.3e cycles_per_block=%lu drops=%lu/%lu\r\n",
                    (unsigned)TEST_SAMPLES, b->max_err, (unsigned long)b->cycles,
                    (unsigned long)raw_drops, (unsigned long)out_drops);
  }
  return (len < size) ? len : size - 1;
}

static void CommTask(void *argument)
{
  (void)argument;
  static OutBlock_t b;
  static char tx[TX_BUF_SIZE];
  uint32_t last_try = 0U;

  uart_printf("\r\n#HW7 CMSIS-DSP FIR demo: fs=%d Hz, fc=%d Hz, taps=%d, block=%d\r\n",
              (int)FIR_FS_HZ, (int)FIR_FC_HZ, FIR_NUM_TAPS, DSP_BLOCK_SIZE);

  wifi_bring_up();
  last_try = osKernelGetTickCount();
  osThreadFlagsSet(sensorTaskHandle, FLAG_START);   /* start sampling now */

  for (;;)
  {
    osMessageQueueGet(outQ, &b, NULL, osWaitForever);
    int len = format_block(&b, tx, sizeof(tx));

    /* 1) WiFi: send before- and after-filter data of the whole block */
    if (tcp_up)
    {
      uint16_t sent = 0U;
      if (WIFI_SendData(0, (uint8_t *)tx, (uint16_t)len, &sent, WIFI_TIMEOUT_MS) != WIFI_STATUS_OK
          || sent != (uint16_t)len)
      {
        WIFI_CloseClientConnection(0);
        tcp_up = 0U;
        last_try = osKernelGetTickCount();
        uart_printf("#WIFI send failed, connection closed (will retry)\r\n");
      }
    }
    else if (wifi_up && (osKernelGetTickCount() - last_try) >= RECONNECT_MS)
    {
      tcp_open();
      last_try = osKernelGetTickCount();
    }

    /* 2) UART: test run always (for verify_test.py); sensor data only as a
     *    fallback while WiFi is down, to keep the CPU/UART load low */
    if (b.mode == MODE_TEST || !tcp_up)
    {
      uart_send(tx, len);
    }
  }
}
