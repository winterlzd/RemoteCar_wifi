#ifndef MAIN_H
#define MAIN_H

#include "stm32f1xx_hal.h"

/* TB6612FNG 方向引脚定义 */
#define PIN_AIN1_PORT GPIOB
#define PIN_AIN1_PIN  GPIO_PIN_12
#define PIN_AIN2_PORT GPIOB
#define PIN_AIN2_PIN  GPIO_PIN_13
#define PIN_BIN1_PORT GPIOB
#define PIN_BIN1_PIN  GPIO_PIN_14
#define PIN_BIN2_PORT GPIOB
#define PIN_BIN2_PIN  GPIO_PIN_15
#define PIN_STBY_PORT GPIOB
#define PIN_STBY_PIN  GPIO_PIN_5

/* LED 引脚定义 (Blue Pill 板载 LED) */
#define PIN_LED_PORT GPIOC
#define PIN_LED_PIN  GPIO_PIN_13

/* PWM 参数 */
#define PWM_MAX        99
#define CMD_TIMEOUT_MS 300

/* 函数声明 */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_TIM3_Init(void);
void MX_USART1_Init(void);
void drive(int left, int right, int brake);
void Error_Handler(void);

#endif /* MAIN_H */
