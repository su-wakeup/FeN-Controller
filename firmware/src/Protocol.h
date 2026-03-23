#pragma once
#include <stdint.h>

// ─── UDP 端口 ────────────────────────────────────────────────
#define UDP_PORT_CTRL   9001   // ESP32 → PC（手柄数据）
#define UDP_PORT_STATUS 9002   // PC → ESP32（状态反馈）

// ─── 控制模式 ────────────────────────────────────────────────
enum CtrlMode : uint8_t {
    MODE_CART    = 0,  // 默认：笛卡尔 XY 平移
    MODE_CART_Z  = 1,  // 按住摇杆：切换 Z 轴
    MODE_ORIENT  = 2,  // 按住 BTN_A：末端姿态
    MODE_JOINT   = 3,  // 按住 BTN_B：关节微调
    MODE_CONFIG  = 4,  // 长按正面键：配置菜单
    MODE_ESTOP   = 5,  // 急停
};

// ─── 按键位掩码 ─────────────────────────────────────────────
#define BTN_L_CLICK  (1 << 0)  // 左摇杆按下
#define BTN_R_CLICK  (1 << 1)  // 右摇杆按下
#define BTN_A        (1 << 2)  // 功能键 A
#define BTN_B        (1 << 3)  // 功能键 B
#define BTN_TOGGLE   (1 << 4)  // 拨杆（ON=1）
#define BTN_FRONT    (1 << 5)  // AtomS3 正面键

// ─── 手柄数据包（ESP32 → PC，共 15 字节）────────────────────
#pragma pack(push, 1)
struct CtrlPacket {
    uint8_t  hdr[2];    // 0xFE 0xFE
    uint8_t  seq;       // 序列号（滚动）
    int16_t  lx;        // 左摇杆 X  -1000~1000
    int16_t  ly;        // 左摇杆 Y  -1000~1000
    int16_t  rx;        // 右摇杆 X  -1000~1000
    int16_t  ry;        // 右摇杆 Y  -1000~1000
    uint8_t  buttons;   // 按键位掩码
    uint8_t  mode;      // CtrlMode
    uint8_t  joint_sel; // 关节模式下选中的关节（0-6 左臂，8-14 右臂）
    uint8_t  checksum;  // XOR(bytes 0..13)
};
#pragma pack(pop)
static_assert(sizeof(CtrlPacket) == 15, "CtrlPacket size mismatch");

// ─── 状态反馈包（PC → ESP32，共 32 字节）─────────────────────
#pragma pack(push, 1)
struct StatusPacket {
    uint8_t  hdr[2];       // 0xFF 0xFF
    uint8_t  flags;        // bit0=左连接 bit1=右连接 bit2=急停 bit3=错误
    float    l_pos[3];     // 左臂末端 XYZ (mm)
    float    r_pos[3];     // 右臂末端 XYZ (mm)
    uint8_t  l_state;      // 0=空闲 1=运动 2=错误
    uint8_t  r_state;
    uint8_t  l_gripper;    // 0-100%
    uint8_t  r_gripper;
    uint8_t  battery;      // 电量 0-100%
    uint8_t  checksum;
};
#pragma pack(pop)
static_assert(sizeof(StatusPacket) == 32, "StatusPacket size mismatch");

// ─── 校验和 ──────────────────────────────────────────────────
inline uint8_t calcChecksum(const uint8_t* data, size_t len) {
    uint8_t cs = 0;
    for (size_t i = 0; i < len; i++) cs ^= data[i];
    return cs;
}
