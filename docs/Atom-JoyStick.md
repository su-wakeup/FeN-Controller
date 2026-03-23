# M5Stack Atom JoyStick

> **Atom JoyStick** 是 M5Stack 推出的一款面向开发者的迷你游戏手柄套件，以 AtomS3（ESP32）为主控，搭载双霍尔传感器摇杆和 0.85 寸彩色 LCD，非常适合用于机器人遥控、无人机操控等应用。

---

## 外观

<p align="center">
  <img src="https://shop.m5stack.com/cdn/shop/files/1_2dc850d2-bf4e-4f3c-8e8d-7c3244d398f2_800x.webp?v=1722411275" width="480"/>
</p>

---

## 硬件组成

| 模块 | 描述 |
|------|------|
| 主控 MCU | **AtomS3**（ESP32-PICO，Xtensa 240MHz，4MB Flash） |
| 协处理器 | STM32F030F4P6（I2C 地址 `0x59`，负责摇杆 ADC 采集） |
| 摇杆 | 双霍尔传感器五向摇杆 × 2（X/Y 轴 + 按下，12-bit ADC） |
| 功能键 | 2 个独立按键（BTN_A / BTN_B） |
| 拨杆 | 1 个拨动开关 |
| 屏幕 | 0.85 寸 LCD（128 × 128 像素，内置于 AtomS3） |
| RGB LED | WS2812C |
| 蜂鸣器 | 内置无源蜂鸣器（5020 Hz） |
| 电池 | 300mAh 高压锂电（4.35V） |
| 充电 IC | TP4067，DC 5V/430mA，约 55 分钟充满 |
| 充电接口 | USB-C |

---

## 无线通信

| 协议 | 规格 |
|------|------|
| WiFi | 802.11 b/g/n（2.4GHz） |
| 蓝牙 | BT 4.2 + BLE |
| ESP-NOW | 内置支持（低延迟点对点） |

本项目使用 **WiFi UDP** 通信（50Hz，延迟 <5ms）。

---

## I2C 寄存器映射（摇杆协处理器 @ 0x59）

| 寄存器 | 内容 | 格式 |
|--------|------|------|
| `0x00` | 摇杆 1（左）X 轴 | uint16 little-endian，0~4095 |
| `0x02` | 摇杆 1（左）Y 轴 | uint16 little-endian，0~4095 |
| `0x20` | 摇杆 2（右）X 轴 | uint16 little-endian，0~4095 |
| `0x22` | 摇杆 2（右）Y 轴 | uint16 little-endian，0~4095 |
| `0x70` | BTN_0（左摇杆按下）| uint8，0=释放 1=按下 |
| `0x71` | BTN_1（右摇杆按下）| uint8 |
| `0x72` | BTN_A | uint8 |
| `0x73` | BTN_B | uint8 |
| `0x60` | 电池电压 | uint16，单位 mV |
| `0xFE` | 固件版本 | uint8 |

AtomS3 I2C 引脚：**SDA = GPIO2，SCL = GPIO1**，频率 400kHz。

---

## 物理规格

| 参数 | 值 |
|------|----|
| 产品尺寸 | 84 × 60 × 31.5 mm |
| 产品重量 | 63.5 g |
| 工作温度 | 0 ~ 40°C |

---

## 开发环境

支持以下开发框架：

- **PlatformIO**（本项目使用，推荐）
- Arduino IDE
- MicroPython
- UIFlow（可视化编程）

依赖库：
```ini
lib_deps =
    m5stack/M5Unified @ ^0.2.2
    m5stack/M5GFX     @ ^0.2.4
```

---

## 在本项目中的按键映射

| 输入 | 功能 |
|------|------|
| 左摇杆 XY | 左臂末端 XY 平移（笛卡尔模式） |
| 右摇杆 XY | 右臂末端 XY 平移 |
| 左摇杆按下 + Y | 左臂 Z 轴控制 |
| 右摇杆按下 + Y | 右臂 Z 轴控制 |
| 按住 BTN_A | 末端姿态模式（Roll/Pitch） |
| 按住 BTN_B | 关节微调模式 |
| BTN_A + BTN_B | 双臂回零位 |
| 拨杆 ON/OFF | 夹爪张开 / 夹紧 |
| 正面键 短按 | 急停 / 恢复 |
| 正面键 长按 1.5s | 进入配置菜单 |

---

## LCD 界面（128×128）

```
┌────────────────┐
│● CARTESIAN XY ●│  ← 模式栏（紫色，两侧连接状态指示点）
│                │
│ LEFT           │
│ X+123          │  ← 左臂末端坐标 (mm)
│ Y-045          │
│ Z+380          │
│                │
│ RIGHT          │
│ X-123          │  ← 右臂末端坐标
│ Y-045          │
│ Z+380          │
│                │
│ ⊕  [■■■ ]  ⊕  │  ← 摇杆可视 + 夹爪状态
└────────────────┘
```

---

## 参考链接

- [官方文档](https://docs.m5stack.com/en/app/Atom%20JoyStick)
- [商店页面](https://shop.m5stack.com/products/atom-joystick-with-m5atoms3)
- [GitHub 库](https://github.com/m5stack/Atom-JoyStick)
