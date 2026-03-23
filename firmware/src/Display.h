#pragma once
#include <M5Unified.h>
#include "Protocol.h"
#include "Config.h"

// AtomS3 LCD: 128x128 像素
#define LCD_W  128
#define LCD_H  128

// 颜色主题
#define C_BG        0x0821   // 深蓝黑
#define C_ACCENT    0xA81F   // 紫色
#define C_GREEN     0x07E0
#define C_RED       0xF800
#define C_YELLOW    0xFFE0
#define C_WHITE     0xFFFF
#define C_GRAY      0x8410
#define C_DARK_GRAY 0x4208

class DisplayManager {
public:
    void begin() {
        M5.Lcd.fillScreen(C_BG);
        M5.Lcd.setTextColor(C_WHITE, C_BG);
        showBoot();
    }

    void showBoot() {
        M5.Lcd.fillScreen(C_BG);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(C_ACCENT);
        M5.Lcd.setTextSize(2);
        M5.Lcd.drawString("FeN", LCD_W/2, 45);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(C_GRAY);
        M5.Lcd.drawString("Dual Arm Robot", LCD_W/2, 68);
        M5.Lcd.drawString("Panbotica", LCD_W/2, 82);
        M5.Lcd.setTextColor(C_WHITE);
        M5.Lcd.drawString("Connecting...", LCD_W/2, 100);
    }

    // 星战字幕滚动动效：近大远小，向上消失在灭点
    // 调用一次播放完整动画（约 4 秒），用于 WiFi AP 模式等待期间
    void showWifiCrawl() {
        // 字幕内容（从下往上出现）
        const char* lines[] = {
            "FeN ROBOT",
            "WIFI SETUP",
            "",
            "Connect to:",
            "FeN-Controller",
            "pw: fenrobot1",
            "",
            "Open browser:",
            "192.168.4.1",
            "",
            "Waiting...",
        };
        const int N = 11;

        // 使用 Sprite 双缓冲避免闪烁
        LGFX_Sprite spr(&M5.Lcd);
        spr.createSprite(LCD_W, LCD_H);

        const int VANISH_Y  = 20;   // 灭点 Y 坐标（屏幕上方）
        const int START_Y   = LCD_H + N * 20;  // 字幕起始位置（屏幕下方外）
        const float SCROLL_SPEED = 1.2f;       // 每帧滚动像素

        float offset = 0;
        uint32_t lastFrame = millis();

        while (offset < (float)(N * 20 + LCD_H)) {
            uint32_t now = millis();
            float dt = (now - lastFrame) / 16.0f;  // 相对于 ~60fps 的时间系数
            lastFrame = now;
            offset += SCROLL_SPEED * dt;

            spr.fillScreen(C_BG);

            // 绘制星点背景
            spr.fillRect(0, 0, LCD_W, VANISH_Y, TFT_BLACK);

            for (int i = 0; i < N; i++) {
                // 每行在"世界空间"的 Y 位置（相对于灭点）
                float worldY = (float)(START_Y - i * 22) - offset;

                // 透视投影：worldY 越大（越靠下）字越大
                // 灭点在 VANISH_Y，底部 LCD_H 处为最大尺寸
                float t = (worldY - VANISH_Y) / (float)(LCD_H - VANISH_Y);
                if (t <= 0.0f || t > 1.4f) continue;  // 超出视锥

                int screenY = (int)(VANISH_Y + t * (LCD_H - VANISH_Y));
                if (screenY < VANISH_Y || screenY > LCD_H + 16) continue;

                // 字体大小随 t 线性变化（0.5 ~ 2）
                float sizeF = 0.4f + t * 1.6f;
                int   sz    = (sizeF > 1.5f) ? 2 : 1;

                // 颜色：顶部（远处）暗，底部（近处）亮
                uint8_t bright = (uint8_t)(60 + t * 195);
                uint16_t color;
                if (i == 0 || i == 1) {
                    // 标题行：金黄色
                    color = spr.color565(bright, (uint8_t)(bright * 0.85f), 0);
                } else if (lines[i][0] == '\0') {
                    continue;
                } else if (strstr(lines[i], "192.168") || strstr(lines[i], "FeN-C")) {
                    // 高亮行：紫色
                    color = spr.color565((uint8_t)(bright * 0.8f), (uint8_t)(bright * 0.6f), bright);
                } else {
                    color = spr.color565(bright, bright, bright);
                }

                spr.setTextSize(sz);
                spr.setTextColor(color);
                spr.setTextDatum(MC_DATUM);
                spr.drawString(lines[i], LCD_W / 2, screenY);
            }

            // 顶部黑色遮罩（制造消失感）
            spr.fillRect(0, 0, LCD_W, VANISH_Y, TFT_BLACK);
            // 底部轻微渐变遮罩（让新文字从下方淡入）
            for (int y = LCD_H - 20; y < LCD_H; y++) {
                uint8_t alpha = (uint8_t)((y - (LCD_H - 20)) * 12);
                spr.drawFastHLine(0, y, LCD_W, spr.color565(0, 0, 0));
            }

            spr.pushSprite(0, 0);
            delay(16);  // ~60fps
        }

        spr.deleteSprite();
    }

    // WiFi 设置静态界面（动画结束后或跳过时显示）
    void showWifiSetup(const char* ssid) {
        M5.Lcd.fillScreen(TFT_BLACK);

        // 顶部标题栏（size 2，明显可读）
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(C_YELLOW);
        M5.Lcd.setTextSize(2);
        M5.Lcd.drawString("WiFi Setup", LCD_W / 2, 14);

        // 分割线
        M5.Lcd.drawFastHLine(0, 26, LCD_W, C_GRAY);

        M5.Lcd.setTextDatum(TL_DATUM);
        M5.Lcd.setTextSize(2);

        // "Connect:" 提示
        M5.Lcd.setTextColor(C_GRAY);
        M5.Lcd.drawString("Connect:", 4, 32);

        // WiFi 名（高亮）
        M5.Lcd.setTextColor(C_WHITE);
        M5.Lcd.drawString("FeN-Ctrl", 4, 52);

        // 分割线
        M5.Lcd.drawFastHLine(0, 72, LCD_W, C_GRAY);

        // "Open:" 提示
        M5.Lcd.setTextColor(C_GRAY);
        M5.Lcd.drawString("Open:", 4, 78);

        // IP 地址（紫色高亮，size 2 完全放得下）
        M5.Lcd.setTextColor(C_ACCENT);
        M5.Lcd.drawString("192.168.4.1", 4, 98);
    }

    // 主界面：显示双臂状态
    void showMain(CtrlMode mode, const StatusPacket& st,
                  int16_t lx, int16_t ly, int16_t rx, int16_t ry) {
        M5.Lcd.fillScreen(C_BG);

        // 顶部：模式栏
        M5.Lcd.fillRect(0, 0, LCD_W, 16, C_ACCENT);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(C_WHITE, C_ACCENT);
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString(modeName(mode), LCD_W/2, 8);

        // 连接状态指示点
        uint16_t lc = (st.flags & 0x01) ? C_GREEN : C_RED;
        uint16_t rc = (st.flags & 0x02) ? C_GREEN : C_RED;
        M5.Lcd.fillCircle(6,  8, 3, lc);
        M5.Lcd.fillCircle(122, 8, 3, rc);

        // 左臂坐标
        M5.Lcd.setTextDatum(TL_DATUM);
        M5.Lcd.setTextColor(0x07FF, C_BG);  // 青色
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString("LEFT", 2, 20);
        drawArmPos(2, 30, st.l_pos, st.l_state);

        // 右臂坐标
        M5.Lcd.setTextColor(0xFD20, C_BG);  // 橙色
        M5.Lcd.drawString("RIGHT", 2, 70);
        drawArmPos(2, 80, st.r_pos, st.r_state);

        // 摇杆可视化（小十字）
        drawMiniStick(18, 113, lx, ly, 0x07FF);
        drawMiniStick(110, 113, rx, ry, 0xFD20);

        // 夹爪状态
        drawGripper(40, 110, st.l_gripper, 0x07FF);
        drawGripper(80, 110, st.r_gripper, 0xFD20);

        // 急停时覆盖红色警告
        if (mode == MODE_ESTOP) {
            M5.Lcd.fillRect(0, 50, LCD_W, 20, C_RED);
            M5.Lcd.setTextDatum(MC_DATUM);
            M5.Lcd.setTextColor(C_WHITE, C_RED);
            M5.Lcd.setTextSize(1);
            M5.Lcd.drawString("!! E-STOP !!", LCD_W/2, 60);
        }
    }

    // 配置菜单
    void showConfig(int item, const AppConfig& cfg) {
        M5.Lcd.fillScreen(C_BG);
        M5.Lcd.fillRect(0, 0, LCD_W, 16, 0x4010);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(C_YELLOW, 0x4010);
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString("CONFIG", LCD_W/2, 8);

        const char* items[] = {
            "WiFi SSID",
            "PC IP",
            "Left Arm IP",
            "Right Arm IP",
            "Speed Cart",
            "Speed Orient",
            "Gripper Type",
            "Auto Discover",
            "Factory Reset",
            "Exit"
        };
        const int N = 10;
        for (int i = 0; i < N; i++) {
            int y = 20 + i * 11;
            if (y > 118) break;
            bool sel = (i == item);
            M5.Lcd.fillRect(0, y-1, LCD_W, 11, sel ? C_ACCENT : C_BG);
            M5.Lcd.setTextDatum(TL_DATUM);
            M5.Lcd.setTextColor(sel ? C_WHITE : C_GRAY, sel ? C_ACCENT : C_BG);
            M5.Lcd.drawString(items[i], 6, y);
            // 当前值（右侧）
            M5.Lcd.setTextDatum(TR_DATUM);
            M5.Lcd.setTextColor(sel ? C_YELLOW : C_DARK_GRAY, sel ? C_ACCENT : C_BG);
            switch(i) {
                case 0: M5.Lcd.drawString(cfg.wifi_ssid[0] ? cfg.wifi_ssid : "?", 124, y); break;
                case 1: M5.Lcd.drawString(cfg.pc_ip, 124, y); break;
                case 2: M5.Lcd.drawString(cfg.left_arm_ip, 124, y); break;
                case 3: M5.Lcd.drawString(cfg.right_arm_ip, 124, y); break;
                case 4: { char buf[8]; snprintf(buf, 8, "%.0f", cfg.speed_cart);  M5.Lcd.drawString(buf, 124, y); break; }
                case 5: { char buf[8]; snprintf(buf, 8, "%.0f", cfg.speed_orient); M5.Lcd.drawString(buf, 124, y); break; }
                case 6: M5.Lcd.drawString(gripperName(cfg.gripper_type), 124, y); break;
                case 7: M5.Lcd.drawString(cfg.auto_discover ? "ON" : "OFF", 124, y); break;
            }
        }
    }

    void showDiscovery(int found) {
        static int dots = 0;
        dots = (dots + 1) % 4;
        M5.Lcd.fillRect(0, 90, LCD_W, 30, C_BG);
        M5.Lcd.setTextDatum(TL_DATUM);
        M5.Lcd.setTextColor(C_YELLOW, C_BG);
        M5.Lcd.setTextSize(1);
        char buf[24];
        snprintf(buf, 24, "Scan%.*s (%d found)", dots, "...", found);
        M5.Lcd.drawString(buf, 4, 94);
    }

    void showMessage(const char* msg, uint16_t color = C_WHITE, int durationMs = 1500) {
        M5.Lcd.fillRect(0, 50, LCD_W, 28, C_BG);
        M5.Lcd.fillRect(2, 52, LCD_W-4, 24, 0x2104);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(color, 0x2104);
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString(msg, LCD_W/2, 64);
        if (durationMs > 0) delay(durationMs);
    }

private:
    void drawArmPos(int x, int y, const float pos[3], uint8_t state) {
        char buf[20];
        uint16_t sc = (state == 1) ? C_YELLOW : (state == 2 ? C_RED : C_WHITE);
        M5.Lcd.setTextColor(sc, C_BG);
        M5.Lcd.setTextSize(1);
        snprintf(buf, 20, "X%+.0f", pos[0]);
        M5.Lcd.drawString(buf, x, y);
        snprintf(buf, 20, "Y%+.0f", pos[1]);
        M5.Lcd.drawString(buf, x, y+10);
        snprintf(buf, 20, "Z%+.0f", pos[2]);
        M5.Lcd.drawString(buf, x, y+20);
    }

    void drawMiniStick(int cx, int cy, int16_t vx, int16_t vy, uint16_t color) {
        const int R = 8;
        M5.Lcd.drawCircle(cx, cy, R, C_DARK_GRAY);
        int dx = map(vx, -1000, 1000, -R+2, R-2);
        int dy = map(vy, -1000, 1000, -R+2, R-2);
        M5.Lcd.fillCircle(cx+dx, cy+dy, 2, color);
    }

    void drawGripper(int x, int y, uint8_t pct, uint16_t color) {
        M5.Lcd.drawRect(x-8, y-4, 16, 8, C_DARK_GRAY);
        int w = map(pct, 0, 100, 0, 14);
        M5.Lcd.fillRect(x-7, y-3, w, 6, color);
    }

    const char* modeName(CtrlMode m) {
        switch(m) {
            case MODE_CART:   return "CARTESIAN XY";
            case MODE_CART_Z: return "CARTESIAN Z";
            case MODE_ORIENT: return "ORIENTATION";
            case MODE_JOINT:  return "JOINT CTRL";
            case MODE_CONFIG: return "CONFIG";
            case MODE_ESTOP:  return "!! E-STOP !!";
            default:          return "UNKNOWN";
        }
    }

    const char* gripperName(int t) {
        switch(t) {
            case 0: return "xArm";
            case 1: return "BIO";
            case 2: return "Vacuum";
            default: return "?";
        }
    }
};
