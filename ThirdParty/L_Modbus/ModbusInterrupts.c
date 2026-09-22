 #include "stm32f1xx_hal.h"

extern void MB_Timer_IRQHandler(void);
extern void MB_Uart_IRQHandler(void);
extern void __real_HAL_UART_IRQHandler(UART_HandleTypeDef *huart);

void __wrap_HAL_UART_IRQHandler(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        MB_Uart_IRQHandler();
        return;
    }

    __real_HAL_UART_IRQHandler(huart);
}

void TIM1_UP_IRQHandler(void)
{
    MB_Timer_IRQHandler();
}