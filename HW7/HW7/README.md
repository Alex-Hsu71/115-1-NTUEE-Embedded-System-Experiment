# HW7 CMSIS-DSP Programming

B-L475E-IOT01A：用 CMSIS-DSP 對 LSM6DSL 加速度計做 FIR 低通濾波。程式分成三個 CMSIS-RTOS2 task，濾波前後的資料透過 WiFi 傳到 PC 即時顯示。

## 主要模組

| 功能 | 檔案 |
|---|---|
| 三個 RTOS task、queue、TIM7 觸發、測試訊號、WiFi 傳送 | `App/Src/app.c` |
| CMSIS-DSP FIR（`arm_fir_f32`）和對照用的純 C FIR | `App/Src/dsp_filter.c`、`App/Inc/dsp_filter.h` |
| FIR 係數（31 taps，fs = 100 Hz，fc = 5 Hz） | `App/Inc/fir_coeffs.h`，由 `tools/fir_design.py` 產生 |
| WiFi 熱點與 PC IP 設定 | `App/Inc/wifi_config.h` |
| 呼叫 `App_Init()`、把 TIM7 callback 轉給 app | `Core/Src/main.c`（USER CODE 區） |
| WiFi 中斷（EXTI1、SPI3） | `Core/Src/stm32l4xx_it.c`（USER CODE 區） |
| 加速度計驅動（BSP） | `Drivers/BSP/` |
| WiFi 模組驅動（ISM43362，來自 HW2） | `Drivers/WiFi/` |
| PC：用測試訊號驗證濾波結果 | `tools/verify_test.py` |
| PC：即時畫原始和濾波後的波形（WiFi 或 UART） | `tools/live_plot.py` |

## 執行

```
cd tools
uv run verify_test.py --port COM7      # 驗證測試訊號（按板子的藍色按鈕重跑）
uv run live_plot.py --tcp 8002         # WiFi 即時顯示（先開這個再重開板子）
uv run live_plot.py --port COM7        # WiFi 沒連上時改用 UART
```

## 換熱點

1. 熱點必須是 **2.4 GHz**，WiFi 模組不支援 5 GHz。
   - iPhone：開啟「最大化相容性」
   - Android：熱點頻段選 2.4 GHz
2. 電腦連上同一個熱點，執行 `ipconfig`，記下 Wi-Fi 網卡的 IPv4 位址。
3. 修改 `App/Inc/wifi_config.h`：
   ```c
   #define WIFI_SSID        "熱點名稱"
   #define WIFI_PASSWORD    "密碼"
   #define REMOTE_IP        {192, 168, 0, 187}   // 第 2 步查到的電腦 IP，用逗號分隔
   #define REMOTE_PORT      8002
   ```
   `wifi_config.h` 不會被 commit。如果這個檔案不存在，就把 `wifi_config.example.h` 複製一份，改名成 `wifi_config.h`。
4. 重新 build 並燒錄。
5. 先執行 `uv run live_plot.py --tcp 8002`，再按板子的 RESET。

如果連不上，用 `uv run python -m serial.tools.miniterm COM7 115200` 看板子的 UART 訊息：
- `cannot join`：SSID 或密碼錯誤，或熱點不是 2.4 GHz
- `TCP connect ... failed`：電腦 IP 錯誤、PC 端沒開，或被 Windows 防火牆擋住

電腦的 IP 由 DHCP 分配，可能會變，每次換網路都要重新確認。
