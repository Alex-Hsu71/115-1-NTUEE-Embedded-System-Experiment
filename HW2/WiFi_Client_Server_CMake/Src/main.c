/**
  ******************************************************************************
  * @file    Wifi/WiFi_Client_Server/src/main.c
  * @author  MCD Application Team
  * @brief   This file provides main program functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private defines -----------------------------------------------------------*/

#define TERMINAL_USE

/* Update SSID and PASSWORD with own Access point settings */
#define SSID     "raa24"
#define PASSWORD "@AlEx#61710&"

uint8_t RemoteIP[] = {192,168,0,187};
#define RemotePORT	8002

#define WIFI_WRITE_TIMEOUT 10000
#define WIFI_READ_TIMEOUT  10000

#define CONNECTION_TRIAL_MAX          10

#if defined (TERMINAL_USE)
#define TERMOUT(...)  printf(__VA_ARGS__)
#else
#define TERMOUT(...)
#endif

/* Private variables ---------------------------------------------------------*/
#if defined (TERMINAL_USE)
extern UART_HandleTypeDef hDiscoUart;
#endif /* TERMINAL_USE */


/* Private function prototypes -----------------------------------------------*/
#if defined (TERMINAL_USE)
#ifdef __GNUC__
/* With GCC, small TERMOUT (option LD Linker->Libraries->Small TERMOUT
   set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
#endif /* TERMINAL_USE */

static void SystemClock_Config(void);
static void Motion_Interrupt_Init(void);



extern  SPI_HandleTypeDef hspi;
static volatile uint8_t motionEventPending = 0;
static volatile uint8_t motionInterruptPending = 0;

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  uint8_t  MAC_Addr[6] = {0};
  uint8_t  IP_Addr[4] = {0};
  int32_t Socket = -1;
  int32_t ret;
  int16_t Trials = CONNECTION_TRIAL_MAX;
  int16_t pDataXYZ[3] = {0};
  float gyroXYZ[3] = {0.0f};
  char sendBuffer[160];
  uint16_t sentLen = 0;

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();
  /* Configure LED2 */
  BSP_LED_Init(LED2);

#if defined (TERMINAL_USE)
  /* Initialize all configured peripherals */
  hDiscoUart.Instance = DISCOVERY_COM1;
  hDiscoUart.Init.BaudRate = 115200;
  hDiscoUart.Init.WordLength = UART_WORDLENGTH_8B;
  hDiscoUart.Init.StopBits = UART_STOPBITS_1;
  hDiscoUart.Init.Parity = UART_PARITY_NONE;
  hDiscoUart.Init.Mode = UART_MODE_TX_RX;
  hDiscoUart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hDiscoUart.Init.OverSampling = UART_OVERSAMPLING_16;
  hDiscoUart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hDiscoUart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  BSP_COM_Init(COM1, &hDiscoUart);

  BSP_ACCELERO_Init();
    BSP_GYRO_Init();
  Motion_Interrupt_Init();
  BSP_ACCELERO_AccGetXYZ(pDataXYZ);
    BSP_GYRO_GetXYZ(gyroXYZ);
  TERMOUT("Accelerometer X: %d, Y: %d, Z: %d\r\n",
          pDataXYZ[0], pDataXYZ[1], pDataXYZ[2]);
    TERMOUT("Gyroscope X: %.2f, Y: %.2f, Z: %.2f\r\n",
      gyroXYZ[0], gyroXYZ[1], gyroXYZ[2]);
#endif /* TERMINAL_USE */

  TERMOUT("****** WIFI Module in TCP Client mode demonstration ****** \n\n");
  TERMOUT("TCP Client Instructions :\n");
  TERMOUT("1- Make sure your Phone is connected to the same network that\n");
  TERMOUT("   you configured using the Configuration Access Point.\n");
  TERMOUT("2- Create a server by using the android application TCP Server\n");
  TERMOUT("   with port(8002).\n");
  TERMOUT("3- Get the Network Name or IP Address of your Android from the step 2.\n\n");



  /*Initialize  WIFI module */
  if(WIFI_Init() ==  WIFI_STATUS_OK)
  {
    TERMOUT("> WIFI Module Initialized.\n");
    if(WIFI_GetMAC_Address(MAC_Addr, sizeof(MAC_Addr)) == WIFI_STATUS_OK)
    {
      TERMOUT("> es-wifi module MAC Address : %X:%X:%X:%X:%X:%X\n",
               MAC_Addr[0],
               MAC_Addr[1],
               MAC_Addr[2],
               MAC_Addr[3],
               MAC_Addr[4],
               MAC_Addr[5]);
    }
    else
    {
      TERMOUT("> ERROR : CANNOT get MAC address\n");
      BSP_LED_On(LED2);
    }

    if( WIFI_Connect(SSID, PASSWORD, WIFI_ECN_WPA2_PSK) == WIFI_STATUS_OK)
    {
      TERMOUT("> es-wifi module connected \n");
      if(WIFI_GetIP_Address(IP_Addr, sizeof(IP_Addr)) == WIFI_STATUS_OK)
      {
        TERMOUT("> es-wifi module got IP Address : %d.%d.%d.%d\n",
               IP_Addr[0],
               IP_Addr[1],
               IP_Addr[2],
               IP_Addr[3]);

        TERMOUT("> Trying to connect to Server: %d.%d.%d.%d:%d ...\n",
               RemoteIP[0],
               RemoteIP[1],
               RemoteIP[2],
               RemoteIP[3],
							 RemotePORT);

        while (Trials--)
        {
          if( WIFI_OpenClientConnection(0, WIFI_TCP_PROTOCOL, "TCP_CLIENT", RemoteIP, RemotePORT, 0) == WIFI_STATUS_OK)
          {
            TERMOUT("> TCP Connection opened successfully.\n");
            Socket = 0;
            break;
          }
        }
        if(Socket == -1)
        {
          TERMOUT("> ERROR : Cannot open Connection\n");
          BSP_LED_On(LED2);
        }
      }
      else
      {
        TERMOUT("> ERROR : es-wifi module CANNOT get IP address\n");
        BSP_LED_On(LED2);
      }
    }
    else
    {
      TERMOUT("> ERROR : es-wifi module NOT connected\n");
      BSP_LED_On(LED2);
    }
  }
  else
  {
    TERMOUT("> ERROR : WIFI Module cannot be initialized.\n");
    BSP_LED_On(LED2);
  }

  while(1)
  {
    if(Socket != -1)
    {
      if(motionInterruptPending != 0U)
      {
        motionInterruptPending = 0U;
        if((SENSOR_IO_Read(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,
                           LSM6DSL_ACC_GYRO_WAKE_UP_SRC) & 0x08U) != 0U)
        {
          motionEventPending = 1U;
        }
      }

      if(motionEventPending != 0U)
      {
        static const char motionEvent[] = "EVENT:MOTION\n";
        motionEventPending = 0U;
        WIFI_SendData(Socket, (uint8_t *)motionEvent,
                      (uint16_t)(sizeof(motionEvent) - 1U),
                      &sentLen, WIFI_WRITE_TIMEOUT);
        TERMOUT("ALERT: significant motion detected\r\n");
      }

      BSP_ACCELERO_AccGetXYZ(pDataXYZ);
      BSP_GYRO_GetXYZ(gyroXYZ);

      int len = snprintf(sendBuffer, sizeof(sendBuffer),
                         "DATA,ACC:%d,%d,%d,GYRO:%.2f,%.2f,%.2f\n",
                         pDataXYZ[0], pDataXYZ[1], pDataXYZ[2],
                         gyroXYZ[0], gyroXYZ[1], gyroXYZ[2]);

      if((len > 0) && (len < (int)sizeof(sendBuffer)))
      {
        ret = WIFI_SendData(Socket, (uint8_t *)sendBuffer,
                            (uint16_t)len, &sentLen, WIFI_WRITE_TIMEOUT);
        if(ret == WIFI_STATUS_OK)
        {
          TERMOUT("Sent to server: %s", sendBuffer);
        }
        else
        {
          TERMOUT("> ERROR : Failed to send accelerometer data\r\n");
          break;
        }
      }

      HAL_Delay(100);
    }
    else
    {
      HAL_Delay(1000);
    }
  }
}

static void Motion_Interrupt_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  uint8_t wakeThreshold = 0x02U;
  uint8_t wakeDuration = 0x00U;
  uint8_t interruptRoute = 0x20U;

  /*
   * On the B-L475E-IOT01A, LSM6DSL INT1 is wired to PD11 (EXTI11), which is
   * a different EXTI line from the WiFi module's DRDY pin (EXTI1/GPIO_PIN_1)
   * already handled elsewhere. A dedicated GPIO/EXTI/NVIC setup is required
   * here, otherwise the wake-up interrupt from the accelerometer is never
   * actually delivered to the CPU.
   */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  GPIO_InitStruct.Pin  = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0x0FU, 0x00U);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* Enable wake-up detection and route the event to LSM6DSL INT1. */
  {
    /*
     * TAP_CFG (0x58) gates ALL "basic interrupt" generation (wake-up,
     * free-fall, 6D, tap, inactivity) behind its INTERRUPTS_ENABLE bit
     * (bit7), which defaults to 0. Without this write, WAKE_UP_THS /
     * WAKE_UP_DUR / MD1_CFG are configured correctly but INT1 never
     * actually asserts, no matter how hard the board is shaken.
     * 0x90 = INTERRUPTS_ENABLE (bit7) | SLOPE_FDS (bit4, selects the
     * high-pass filter for the wake-up path), per ST AN5040.
     */
    uint8_t tapConfig = 0x90U;
    SENSOR_IO_WriteMultiple(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,
                             LSM6DSL_ACC_GYRO_TAP_CFG1,
                             &tapConfig, 1U);
  }
  SENSOR_IO_WriteMultiple(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,
                           LSM6DSL_ACC_GYRO_WAKE_UP_THS,
                           &wakeThreshold, 1U);
  SENSOR_IO_WriteMultiple(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,
                           LSM6DSL_ACC_GYRO_WAKE_UP_DUR,
                           &wakeDuration, 1U);
  SENSOR_IO_WriteMultiple(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,
                           LSM6DSL_ACC_GYRO_MD1_CFG,
                           &interruptRoute, 1U);
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (MSI)
  *            SYSCLK(Hz)                     = 80000000
  *            HCLK(Hz)                       = 80000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            MSI Frequency(Hz)              = 4000000
  *            PLL_M                          = 1
  *            PLL_N                          = 40
  *            PLL_R                          = 2
  *            PLL_P                          = 7
  *            PLL_Q                          = 4
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* MSI is enabled after System reset, activate PLL with MSI as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLP = 7;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
}

#if defined (TERMINAL_USE)
/**
  * @brief  Retargets the C library TERMOUT function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART1 and Loop until the end of transmission */
  HAL_UART_Transmit(&hDiscoUart, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}
#endif /* TERMINAL_USE */

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: TERMOUT("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @brief  EXTI line detection callback.
  * @param  GPIO_Pin: Specifies the port pin connected to corresponding EXTI line.
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
    case (GPIO_PIN_1):
    {
      /* ISM43362 WiFi module DRDY line (EXTI1) - unrelated to motion. */
      SPI_WIFI_ISR();
      break;
    }
    case (GPIO_PIN_11):
    {
      /* LSM6DSL INT1 line (EXTI11) - significant motion wake-up event. */
      motionInterruptPending = 1U;
      break;
    }
    default:
    {
      break;
    }
  }
}

void SPI3_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi);
}

/**
  * @brief  This function handles EXTI lines 10 to 15 interrupt requests,
  *         which includes EXTI11 used by the LSM6DSL INT1 (motion) pin.
  * @param  None
  * @retval None
  */
void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
}