# STM32 遥控小车下位机

基于 STM32F103C8T6 (Blue Pill) 的遥控小车控制程序，通过 TB6612FNG 电机驱动模块控制两个直流电机，支持串口指令控制。

## 硬件连接

### TB6612FNG 驱动模块
| STM32 引脚 | 功能 | TB6612 引脚 |
|------------|------|-------------|
| PA6 (TIM3_CH1) | PWM 输出 | PWMA (左轮速度) |
| PA7 (TIM3_CH2) | PWM 输出 | PWMB (右轮速度) |
| PB12 | GPIO 输出 | AIN1 (左轮方向) |
| PB13 | GPIO 输出 | AIN2 (左轮方向) |
| PB14 | GPIO 输出 | BIN1 (右轮方向) |
| PB15 | GPIO 输出 | BIN2 (右轮方向) |
| PB5 | GPIO 输出 | STBY (使能，高有效) |

### 串口通信 (连接树莓派)
| STM32 引脚 | 功能 | 树莓派引脚 |
|------------|------|------------|
| PA9 (USART1_TX) | 发送 | GPIO15 (RX) |
| PA10 (USART1_RX) | 接收 | GPIO14 (TX) |

串口参数：**115200 波特率，8N1**

## 串口控制协议

发送格式：`M,<left>,<right>,<brake>\n`

- `left` / `right`：-255 ~ +255（正数前进，负数后退，0 停止）
- `brake`：`1` = 短刹（电机两端接高），`0` = 滑行（电机两端接低）

示例：
```
M,200,200,0\n      // 两轮同速前进（滑行停止）
M,-150,150,0\n     // 原地左转
M,0,0,1\n          // 立即刹车
```

## 超时保护

如果 **300ms** 内未收到新的控制指令，小车将自动短刹，防止失控。

## 构建与上传

本项目使用 [PlatformIO](https://platformio.org/) 构建。

```bash
# 安装 PlatformIO CLI（如尚未安装）
pip install platformio

# 编译
pio run

# 上传（需连接 ST-Link）
pio run --target upload

# 串口监视（可选）
pio device monitor
```

## 项目结构

```
remote_car/
├── platformio.ini          # PlatformIO 配置文件
├── include/
│   ├── main.h              # 主头文件（引脚定义、函数声明）
│   └── stm32f1xx_hal_conf.h  # HAL 库配置
├── src/
│   ├── main.c              # 主程序
│   └── stm32f1xx_it.c      # 中断处理
├── lib/                    # 自定义库（当前为空）
└── test/                   # 测试文件（当前为空）
```

## 开发环境

- **MCU**: STM32F103C8T6 (Blue Pill)
- **开发框架**: STM32Cube + HAL 库
- **构建系统**: PlatformIO
- **调试器**: ST-Link V2

## 许可证

MIT License