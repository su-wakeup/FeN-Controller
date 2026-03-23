# FeN Robot Controller

<p align="center">
  <img src="http://www.panbotica.com/pc/static/img/no3_1.jpg" width="600"/>
</p>

<p align="center">
  <b>M5Stack Atom JoyStick → FeN 双臂机器人遥控系统</b><br/>
  by <a href="http://www.panbotica.com">Panbotica（全世萝卜）</a>
</p>

---

## 文档

| | |
|--|--|
| [FeN 双臂机器人介绍](docs/FeN-Robot.md) | 机器人规格、特性、部署案例 |
| [M5Stack Atom JoyStick 介绍](docs/Atom-JoyStick.md) | 手柄硬件规格、I2C 协议、按键映射 |

---

## 系统架构

```
┌─────────────────────────────────┐
│   M5Stack Atom JoyStick         │
│   (ESP32 AtomS3 + 双霍尔摇杆)   │
│   0.85" LCD  •  WiFi  •  BT     │
└──────────────┬──────────────────┘
               │ WiFi UDP 50Hz
               ▼
┌─────────────────────────────────┐
│   Ubuntu ROS PC  (fen_bridge)   │
│   ROS1 Noetic / ROS2 Humble     │
│   xArm Python SDK               │
└────────┬──────────────┬─────────┘
         │ TCP/IP       │ TCP/IP
         ▼              ▼
   左臂 xArm 6/7   右臂 xArm 6/7
   192.168.1.202   192.168.1.201
         │              │
         └──────┬────────┘
                ▼
         FeN 双臂机器人
         (12~14 自由度)
```

---

## 控制模式

| 输入 | 默认（笛卡尔 XY） | 按住 BTN_A（姿态） | 按住 BTN_B（关节） |
|------|------|------|------|
| 左摇杆 XY | 左臂末端 XY 平移 | 左臂 Roll / Pitch | 选关节（上下滚动） |
| 右摇杆 XY | 右臂末端 XY 平移 | 右臂 Roll / Pitch | 调关节角度 |
| 左摇杆按下 + Y | 左臂 Z 轴 | — | — |
| 右摇杆按下 + Y | 右臂 Z 轴 | — | — |
| 拨杆 ON / OFF | 夹爪 开 / 合 | | |
| 正面键 短按 | 急停 / 恢复 | | |
| 正面键 长按 1.5s | 进入配置菜单 | | |
| BTN_A + BTN_B | 双臂回零位 | | |

---

## 快速开始

### 1. 固件烧录（ESP32 AtomS3）

```bash
cd firmware
pip install platformio
pio run --target upload
```

首次烧录后，手柄会进入 AP 模式：
- 连接 WiFi：`FeN-Controller`（密码：`fenrobot1`）
- 浏览器访问 `192.168.4.1` 填写 WiFi 和各设备 IP

### 2. PC 端 ROS Bridge

```bash
cd ros_bridge
pip install -r requirements.txt

# 自动检测 ROS 版本，自动发现机械臂
python3 fen_bridge.py

# 手动指定 IP，禁用自动发现
python3 fen_bridge.py --left-ip 192.168.1.202 --right-ip 192.168.1.201 --no-discover

# 指定 ROS 版本（auto | ros1 | ros2 | none）
python3 fen_bridge.py --ros-version ros2
```

### 3. 配置文件

编辑 `ros_bridge/config/config.yaml`：

```yaml
arms:
  left:
    ip: "192.168.1.202"
    dof: 6          # 或 7
  right:
    ip: "192.168.1.201"
    dof: 6

gripper:
  type: "xarm"      # xarm | bio | vacuum

discovery:
  enabled: true     # 开启局域网自动发现
  subnet: "192.168.1"
```

---

## 项目结构

```
fen-controller/
├── firmware/                  # ESP32 AtomS3 PlatformIO 项目
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp           # 主循环 + 控制模式状态机
│       ├── Protocol.h         # UDP 包定义（固件 / Python 共享协议）
│       ├── JoyStick.h         # I2C 摇杆驱动（STM32 @ 0x59）
│       ├── Display.h          # LCD 界面管理
│       ├── Network.h          # WiFi + UDP + 网页首次配置
│       └── Config.h           # NVS 持久化配置
├── ros_bridge/                # Python ROS 桥接
│   ├── fen_bridge.py          # 主程序（支持 ROS1 / ROS2 / 纯 Python）
│   ├── arm_controller.py      # 双臂 xArm 控制逻辑
│   ├── protocol.py            # UDP 协议解析
│   ├── discovery.py           # 局域网自动发现 xArm
│   ├── requirements.txt
│   └── config/
│       └── config.yaml        # 默认配置
└── docs/
    ├── FeN-Robot.md           # FeN 双臂机器人介绍
    └── Atom-JoyStick.md       # M5Stack Atom JoyStick 介绍
```

---

## 硬件清单

| 硬件 | 型号 | 备注 |
|------|------|------|
| 手柄 | M5Stack Atom JoyStick | AtomS3 主控 + STM32 协处理器 |
| 左臂 | UFactory xArm 6 或 xArm 7 | 默认 IP `192.168.1.202` |
| 右臂 | UFactory xArm 6 或 xArm 7 | 默认 IP `192.168.1.201` |
| 中控 PC | Ubuntu Linux | ROS1 Noetic 或 ROS2 Humble |
| 夹爪 | xArm 标准夹爪 / BIO 夹爪 / 真空吸盘 | 可配置 |

---

<p align="center">
  <img src="http://www.panbotica.com/pc/static/img/no3_2.jpg" width="600"/>
</p>

<p align="center">
  <sub>FeN 双臂机器人 — Panbotica © 2022</sub>
</p>
