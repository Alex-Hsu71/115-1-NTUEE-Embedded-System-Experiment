#ifndef APP_H
#define APP_H

#include "stm32l4xx_hal.h"

/* Create the application tasks / queues. Call after osKernelInitialize(). */
void App_Init(void);

/* Forward HAL_TIM_PeriodElapsedCallback() here (called from TIM7 ISR). */
void App_TimerCallback(TIM_HandleTypeDef *htim);

#endif /* APP_H */
