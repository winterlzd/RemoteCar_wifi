/**
 * STM32F1xx 中断处理
 * 
 * 必须实现：
 * - SysTick_Handler: HAL 时基
 * - USART1_IRQHandler: 串口1中断
 */

#include "stm32f1xx_hal.h"

/* 外部句柄声明 (在 main.c 中定义) */
extern UART_HandleTypeDef huart1;

/**
 * SysTick 中断处理
 * HAL 时基，每 1ms 触发一次
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/**
 * USART1 中断处理
 * 转发给 HAL 库处理
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
