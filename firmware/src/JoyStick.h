#pragma once
#include <Wire.h>
#include <Arduino.h>

// Atom JoyStick STM32 共处理器 I2C 地址
#define JOYSTICK_I2C_ADDR  0x59
#define JOYSTICK_SDA_PIN   2
#define JOYSTICK_SCL_PIN   1
#define JOYSTICK_I2C_FREQ  400000

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
        Wire.begin(JOYSTICK_SDA_PIN, JOYSTICK_SCL_PIN, JOYSTICK_I2C_FREQ);
        delay(50);
        // 检查设备是否存在
        Wire.beginTransmission(JOYSTICK_I2C_ADDR);
        return Wire.endTransmission() == 0;
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

        s.l_btn = readBtn(0x70);
        s.r_btn = readBtn(0x71);
        s.btn_a = readBtn(0x72);
        s.btn_b = readBtn(0x73);

        s.battery_mv = readReg16(REG_BAT_V);
        return s;
    }

    uint8_t firmwareVersion() {
        return readReg8(REG_FW_VER);
    }

private:
    // 将 0-4095 映射到 -1000~1000（以标定中心为零点）
    int16_t normalize(uint16_t raw, uint16_t center) {
        int32_t v = (int32_t)raw - (int32_t)center;
        // 中心区域约 ±200 ADC units 对应满量程 ±1000
        v = constrain(v * 1000L / 2048L, -1000L, 1000L);
        return (int16_t)v;
    }

    uint16_t readReg16(uint8_t reg) {
        Wire.beginTransmission(JOYSTICK_I2C_ADDR);
        Wire.write(reg);
        Wire.endTransmission(false);
        Wire.requestFrom(JOYSTICK_I2C_ADDR, (uint8_t)2);
        if (Wire.available() < 2) return 2048;
        uint8_t lo = Wire.read();
        uint8_t hi = Wire.read();
        return (uint16_t)(hi << 8 | lo);  // little-endian
    }

    uint8_t readReg8(uint8_t reg) {
        Wire.beginTransmission(JOYSTICK_I2C_ADDR);
        Wire.write(reg);
        Wire.endTransmission(false);
        Wire.requestFrom(JOYSTICK_I2C_ADDR, (uint8_t)1);
        return Wire.available() ? Wire.read() : 0;
    }

    bool readBtn(uint8_t reg) {
        return readReg8(reg) != 0;
    }
};
