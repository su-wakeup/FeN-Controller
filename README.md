# FeN Robot Controller
**M5Stack Atom JoyStick → xArm 6/7 双臂遥控系统**
by [Panbotica](https://www.panbotica.com)

## 架构

```
Atom JoyStick (ESP32 AtomS3)
    ↓ WiFi UDP 50Hz
Ubuntu ROS PC (fen_bridge.py)
    ├─ 自动发现 / 配置文件
    ├─→ TCP/IP → 左臂 xArm (192.168.1.202)
    └─→ TCP/IP → 右臂 xArm (192.168.1.201)
```

## 控制方案

| 输入 | 默认模式 | BTN_A（姿态） | BTN_B（关节） |
|------|----------|---------------|---------------|
| 左摇杆 XY | 左臂末端 XY 平移 | 左臂 Roll/Pitch | 选关节 |
| 右摇杆 XY | 右臂末端 XY 平移 | 右臂 Roll/Pitch | 调关节角度 |
| 左摇杆按下 + Y | 左臂 Z 轴 | - | - |
| 右摇杆按下 + Y | 右臂 Z 轴 | - | - |
| 拨杆 ON/OFF | 夹爪 开/合 | | |
| 正面键短按 | 急停 / 恢复 | | |
| 正面键长按 1.5s | 进入配置菜单 | | |
| BTN_A + BTN_B | 回零位 | | |

## 快速开始

### 1. 固件烧录（ESP32 AtomS3）

```bash
cd firmware
pip install platformio
pio run --target upload
```

首次烧录后，手柄会进入 AP 模式：
- 连接 WiFi: `FeN-Controller`（密码: `fenrobot1`）
- 打开浏览器访问 `192.168.4.1` 配置 WiFi 和 IP

### 2. PC 端 ROS Bridge

```bash
cd ros_bridge
pip install -r requirements.txt

# 自动检测 ROS 版本，自动发现机械臂
python3 fen_bridge.py

# 指定配置
python3 fen_bridge.py --config config/config.yaml

# 手动指定 IP，禁用发现
python3 fen_bridge.py --left-ip 192.168.1.202 --right-ip 192.168.1.201 --no-discover

# ROS2 模式
python3 fen_bridge.py --ros-version ros2

# 纯 Python 模式（无需 ROS）
python3 fen_bridge.py --ros-version none
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

## 项目结构

```
fen-controller/
├── firmware/                  # ESP32 AtomS3 PlatformIO 项目
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp           # 主循环 + 状态机
│       ├── Protocol.h         # UDP 包定义（与 Python 端共享）
│       ├── JoyStick.h         # I2C 摇杆驱动（STM32 @ 0x59）
│       ├── Display.h          # LCD 界面管理
│       ├── Network.h          # WiFi + UDP + Web 配置
│       └── Config.h           # NVS 持久化配置
└── ros_bridge/                # Python ROS 桥接
    ├── fen_bridge.py          # 主程序入口
    ├── arm_controller.py      # 双臂控制逻辑
    ├── protocol.py            # UDP 协议解析
    ├── discovery.py           # 局域网自动发现
    ├── requirements.txt
    └── config/
        └── config.yaml        # 默认配置
```
