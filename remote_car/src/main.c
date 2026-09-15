/**
 * 遥控小车下位机 - STM32F103C8T6
 * 
 * 硬件连接：
 *   TB6612FNG 驱动模块
 *   - PA6 (TIM3_CH1) -> PWMA (左轮)
 *   - PA7 (TIM3_CH2) -> PWMB (右轮)
 *   - PB12/PB13      -> AIN1/AIN2 (左方向)
 *   - PB14/PB15      -> BIN1/BIN2 (右方向)
 *   - PB5            -> STBY (使能)
 * 
 *   串口通信 (连接树莓派)
 *   - PA9  (USART1_TX) -> 树莓派 RX (GPIO15)
 *   - PA10 (USART1_RX) -> 树莓派 TX (GPIO14)
 * 
 * 协议：M,<left>,<right>,<brake>\n
 *   left/right: -255 ~ +255 (正=前进，负=后退)
 *   brake: 1=短刹，0=滑行
 */

#include "main.h"
#include <stdio.h>
#include <string.h>

/* 外设句柄 (非 static，stm32f1xx_it.c 需要 extern 访问) */
static TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart1;

/* 接收缓冲 */
static volatile uint32_t last_cmd_ms;
static volatile uint32_t led_on_ms;        /* LED 点亮时刻 */
static char rx_line[64];
static uint8_t rx_idx;
static uint8_t rx_byte;

/**
 * 系统时钟配置
 * HSE 8MHz -> PLL x9 = 72MHz
 * APB1 = 36MHz, APB2 = 72MHz
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

/**
 * GPIO 初始化
 * PB12-15: TB6612 方向控制 (推挽输出)
 * PB5: STBY 使能 (推挽输出)
 * PA6/PA7: TIM3 PWM (复用推挽)
 * PA9: USART1 TX (复用推挽)
 * PA10: USART1 RX (上拉输入)
 */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PB5, PB12-15 推挽输出 */
    g.Pin = GPIO_PIN_5 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);

    /* PC13 推挽输出 (板载 LED) */
    g.Pin = GPIO_PIN_13;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);

    /* 初始状态：停止并短刹 */
    HAL_GPIO_WritePin(PIN_STBY_PORT, PIN_STBY_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PIN_AIN1_PORT, PIN_AIN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PIN_AIN2_PORT, PIN_AIN2_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PIN_BIN1_PORT, PIN_BIN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PIN_BIN2_PORT, PIN_BIN2_PIN, GPIO_PIN_SET);
    
    /* LED 初始状态：熄灭 (PC13 低电平有效，HIGH=灭) */
    HAL_GPIO_WritePin(PIN_LED_PORT, PIN_LED_PIN, GPIO_PIN_SET);
}

/**
 * TIM3 PWM 初始化
 * PSC=71, ARR=99 -> 72MHz / 72 / 100 = 10kHz
 * CH1 (PA6): 左轮 PWM
 * CH2 (PA7): 右轮 PWM
 */
void MX_TIM3_Init(void)
{
    TIM_OC_InitTypeDef oc = {0};
    __HAL_RCC_TIM3_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 71;          /* 72MHz / 72 = 1MHz */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 99;             /* 1MHz / 100 = 10kHz */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim3);

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

/**
 * USART1 初始化
 * 115200 8N1，RXNE 中断接收
 */
void MX_USART1_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_9;
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_10;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &g);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(&huart1);

    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

/**
 * 速度值转 PWM 占空比
 * 将 -255~255 映射到 0~PWM_MAX(99)
 */
static uint16_t speed_to_ccr(int speed)
{
    if (speed < 0) speed = -speed;
    if (speed > 255) speed = 255;
    return (uint16_t)((speed * PWM_MAX) / 255);
}

/**
 * 单轮驱动控制
 * @param p1, n1  方向引脚1端口和引脚
 * @param p2, n2  方向引脚2端口和引脚
 * @param ch      TIM 通道 (TIM_CHANNEL_1 或 TIM_CHANNEL_2)
 * @param speed   速度值 (-255 ~ +255)
 * @param brake_when_zero  速度为0时是否短刹
 */
static void set_wheel(GPIO_TypeDef *p1, uint16_t n1,
                      GPIO_TypeDef *p2, uint16_t n2,
                      uint32_t ch, int speed, int brake_when_zero)
{
    if (speed > 255) speed = 255;
    if (speed < -255) speed = -255;

    if (speed > 0) {
        /* 正转: IN1=1, IN2=0 */
        HAL_GPIO_WritePin(p1, n1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(p2, n2, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&htim3, ch, speed_to_ccr(speed));
    } else if (speed < 0) {
        /* 反转: IN1=0, IN2=1 */
        HAL_GPIO_WritePin(p1, n1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(p2, n2, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim3, ch, speed_to_ccr(speed));
    } else {
        /* 速度为0 */
        __HAL_TIM_SET_COMPARE(&htim3, ch, 0);
        if (brake_when_zero) {
            /* 短刹: IN1=1, IN2=1 */
            HAL_GPIO_WritePin(p1, n1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(p2, n2, GPIO_PIN_SET);
        } else {
            /* 滑行: IN1=0, IN2=0 */
            HAL_GPIO_WritePin(p1, n1, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(p2, n2, GPIO_PIN_RESET);
        }
    }
}

/**
 * 双轮驱动控制
 * @param left   左轮速度 (-255 ~ +255)
 * @param right  右轮速度 (-255 ~ +255)
 * @param brake  速度为0时: 1=短刹, 0=滑行
 */
void drive(int left, int right, int brake)
{
    set_wheel(PIN_AIN1_PORT, PIN_AIN1_PIN, PIN_AIN2_PORT, PIN_AIN2_PIN,
              TIM_CHANNEL_1, left, brake);
    set_wheel(PIN_BIN1_PORT, PIN_BIN1_PIN, PIN_BIN2_PORT, PIN_BIN2_PIN,
              TIM_CHANNEL_2, right, brake);
}

/**
 * 解析接收到的命令行
 * 协议: M,<left>,<right>,<brake>\n
 */
static void parse_line(char *line)
{
    int left = 0, right = 0, brake = 0;
    if (line[0] != 'M') return;
    if (sscanf(line, "M,%d,%d,%d", &left, &right, &brake) != 3) return;
    
    /* 限幅 */
    if (left > 255) left = 255;
    if (left < -255) left = -255;
    if (right > 255) right = 255;
    if (right < -255) right = -255;
    
    drive(left, right, brake);
    last_cmd_ms = HAL_GetTick();
}

/**
 * UART 接收完成回调
 * 逐字符接收，遇换行符解析整行
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;
    
    char c = (char)rx_byte;
    if (c == '\n' || c == '\r') {
        if (rx_idx > 0) {
            rx_line[rx_idx] = '\0';
            parse_line(rx_line);
            rx_idx = 0;
            /* 收到完整命令，点亮 LED (PC13 低电平有效，LOW=亮) */
            HAL_GPIO_WritePin(PIN_LED_PORT, PIN_LED_PIN, GPIO_PIN_RESET);
            led_on_ms = HAL_GetTick();
        }
    } else if (rx_idx < sizeof(rx_line) - 1) {
        rx_line[rx_idx++] = c;
    }
    
    /* 继续接收下一个字节 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

/**
 * 错误处理函数
 */
void Error_Handler(void)
{
    __disable_irq();
    /* 停止电机 */
    drive(0, 0, 1);
    while (1) {
        /* 死循环，等待复位 */
    }
}

/**
 * 主函数
 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    
    /* 外设初始化 */
    MX_GPIO_Init();
    MX_TIM3_Init();
    MX_USART1_Init();
    
    /* 使能 TB6612 */
    HAL_GPIO_WritePin(PIN_STBY_PORT, PIN_STBY_PIN, GPIO_PIN_SET);
    
    /* 初始停止 */
    drive(0, 0, 1);
    last_cmd_ms = HAL_GetTick();
    led_on_ms = 0;
    
    /* 主循环 */
    while (1) {
        /* LED 亮 100ms 后自动熄灭 (PC13 低电平有效，HIGH=灭) */
        if (led_on_ms && (HAL_GetTick() - led_on_ms) > 100) {
            HAL_GPIO_WritePin(PIN_LED_PORT, PIN_LED_PIN, GPIO_PIN_SET);
            led_on_ms = 0;
        }
        
        /* 超时检测：300ms 无命令则短刹 */
        if ((HAL_GetTick() - last_cmd_ms) > CMD_TIMEOUT_MS) {
            drive(0, 0, 1);
            last_cmd_ms = HAL_GetTick();
        }
    }
}
