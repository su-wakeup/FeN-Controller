#pragma once
#include <M5Unified.h>

// Atom JoyStick STM32 共处理器 I2C 地址
#define JOYSTICK_I2C_ADDR  0x59
#define JOYSTICK_I2C_FREQ  400000UL  // STM32 支持 Fast Mode

// 寄存器地址
#define REG_JOY1_X_12  0x00   // 摇杆1 X轴 12-bit (2字节 little-endian, 0-4095)
#define REG_JOY1_Y_12  0x02   // 摇杆1 Y轴 12-bit
#define REG_JOY2_X_12  0x20   // 摇杆2 X轴 12-bit
#define REG_JOY2_Y_12  0x22   // 摇杆2 Y轴 12-bit
#define REG_BTN_BASE   0x70   // 按键基址：0x70=BTN0, 0x71=BTN1, 0x72=BTNA, 0x73=BTNB
#define REG_BAT_V      0x60   // 电池电压 (mV, 2字节)
#define REG_FW_VER     0xFE   // 固件版本

struct JoyState {
    int16_t lx, ly;    // 左摇杆，归一化 -1000~1000
    int16_t rx, ry;    // 右摇杆，归一化 -1000~1000
    bool    l_btn;     // 左摇杆按下
    bool    r_btn;     // 右摇杆按下
    bool    btn_a;
    bool    btn_b;
    uint16_t battery_mv;
};

class JoyStickDriver {
public:
    // 中心点（上电时标定）
    uint16_t cal_lx = 2048, cal_ly = 2048;
    uint16_t cal_rx = 2048, cal_ry = 2048;

    bool begin() {
        // 使用 M5Unified 的 In_I2C，它已由 M5.begin() 在正确引脚上初始化
        // 无需（也不应）再调用 Wire.begin()，避免与 M5Unified 的 I2C 驱动冲突
        uint8_t dummy = 0;
        return M5.In_I2C.readRegister(JOYSTICK_I2C_ADDR, REG_FW_VER,
                                       &dummy, 1, JOYSTICK_I2C_FREQ);
    }

    // 开机自动标定中心点（保持摇杆居中）
    void calibrate() {
        cal_lx = readReg16(REG_JOY1_X_12);
        cal_ly = readReg16(REG_JOY1_Y_12);
        cal_rx = readReg16(REG_JOY2_X_12);
        cal_ry = readReg16(REG_JOY2_Y_12);
    }

    JoyState read() {
        JoyState s{};
        uint16_t raw_lx = readReg16(REG_JOY1_X_12);
        uint16_t raw_ly = readReg16(REG_JOY1_Y_12);
        uint16_t raw_rx = readReg16(REG_JOY2_X_12);
        uint16_t raw_ry = readReg16(REG_JOY2_Y_12);

        s.lx = normalize(raw_lx, cal_lx);
        s.ly = normalize(raw_ly, cal_ly);
        s.rx = normalize(raw_rx, cal_rx);
        s.ry = normalize(raw_ry, cal_ry);

        s.l_btn = readReg8(0x70) != 0;
        s.r_btn = readReg8(0x71) != 0;
        s.btn_a = readReg8(0x72) != 0;
        s.btn_b = readReg8(0x73) != 0;

        s.battery_mv = readReg16(REG_BAT_V);
        return s;
    }

    uint8_t firmwareVersion() {
        return readReg8(REG_FW_VER);
    }

private:
    int16_t normalize(uint16_t raw, uint16_t center) {
        int32_t v = (int32_t)raw - (int32_t)center;
        v = constrain(v * 1000L / 2048L, -1000L, 1000L);
        return (int16_t)v;
    }

    uint16_t readReg16(uint8_t reg) {
        uint8_t buf[2] = {0x00, 0x08};  // default = 2048 (little-endian)
        M5.In_I2C.readRegister(JOYSTICK_I2C_ADDR, reg, buf, 2, JOYSTICK_I2C_FREQ);
        return (uint16_t)(buf[1] << 8 | buf[0]);
    }

    uint8_t readReg8(uint8_t reg) {
        uint8_t buf = 0;
        M5.In_I2C.readRegister(JOYSTICK_I2C_ADDR, reg, &buf, 1, JOYSTICK_I2C_FREQ);
        return buf;
    }
};
