#pragma once
#include <M5Unified.h>
#include "Protocol.h"
#include "Config.h"
#include "logo_panbotica.h"

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
#define C_CYAN      0x07FF
#define C_ORANGE    0xFD20

class DisplayManager {
public:
    void begin() {
        M5.Lcd.fillScreen(C_BG);
        M5.Lcd.setTextColor(C_WHITE, C_BG);
        showBoot();
    }

    // ── 开机动画 ──────────────────────────────────────────────────
    // 机器人剪影 + "FeN RC" 标题 + Panbotica logo
    // 动画约 2 秒：双臂从中间向两侧展开，标题淡入
    void showBoot() {
        LGFX_Sprite spr(&M5.Lcd);
        spr.createSprite(LCD_W, LCD_H);

        // 机器人剪影各部件坐标（以 cx=64, cy=62 为中心）
        const int cx = 64;
        const int bodyY = 52;

        for (int frame = 0; frame < 48; frame++) {
            float t = frame / 47.0f;  // 0→1
            // 缓动：ease-out cubic
            float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);

            spr.fillScreen(C_BG);

            // 星点背景（3颗固定小星）
            spr.fillCircle(15, 20, 1, C_DARK_GRAY);
            spr.fillCircle(100, 35, 1, C_DARK_GRAY);
            spr.fillCircle(55, 10, 1, C_DARK_GRAY);

            // ── 机器人主体剪影（简洁几何风格）──
            uint16_t bodyCol = spr.color565(
                (uint8_t)(30 + ease * 80),
                (uint8_t)(20 + ease * 40),
                (uint8_t)(60 + ease * 120));

            // 头部
            spr.fillRoundRect(cx - 8, bodyY - 22, 16, 14, 3, bodyCol);
            // 颈
            spr.fillRect(cx - 3, bodyY - 9, 6, 5, bodyCol);
            // 躯干
            spr.fillRoundRect(cx - 12, bodyY - 4, 24, 20, 3, bodyCol);
            // 腿
            spr.fillRect(cx - 9, bodyY + 16, 6, 10, bodyCol);
            spr.fillRect(cx + 3,  bodyY + 16, 6, 10, bodyCol);
            // 脚
            spr.fillRect(cx - 11, bodyY + 25, 9, 4, bodyCol);
            spr.fillRect(cx + 2,  bodyY + 25, 9, 4, bodyCol);

            // 眼睛（动画结束后亮起）
            if (ease > 0.6f) {
                uint8_t eyeB = (uint8_t)((ease - 0.6f) / 0.4f * 255);
                uint16_t eyeCol = spr.color565(0, eyeB, eyeB);
                spr.fillCircle(cx - 3, bodyY - 16, 2, eyeCol);
                spr.fillCircle(cx + 3, bodyY - 16, 2, eyeCol);
            }

            // ── 双臂：从躯干中心向两侧展开 ──
            // 手臂展开距离随 ease 变化（0 → 最大）
            int armSpread = (int)(ease * 26);
            // 左臂（分3段：上臂、肘、前臂）
            int lShoulderX = cx - 12;
            int lElbowX    = lShoulderX - armSpread;
            int lWristX    = lElbowX - (int)(ease * 8);
            int armY       = bodyY + 2;
            spr.drawLine(lShoulderX, armY, lElbowX, armY + 8, bodyCol);
            spr.drawLine(lElbowX, armY + 8, lWristX, armY + 14, bodyCol);
            spr.fillCircle(lWristX, armY + 14, 2, bodyCol);  // 末端效应器
            // 右臂
            int rShoulderX = cx + 12;
            int rElbowX    = rShoulderX + armSpread;
            int rWristX    = rElbowX + (int)(ease * 8);
            spr.drawLine(rShoulderX, armY, rElbowX, armY + 8, bodyCol);
            spr.drawLine(rElbowX, armY + 8, rWristX, armY + 14, bodyCol);
            spr.fillCircle(rWristX, armY + 14, 2, bodyCol);

            // ── 标题文字（ease > 0.3 开始淡入）──
            if (ease > 0.3f) {
                float ta = (ease - 0.3f) / 0.7f;
                uint8_t tb = (uint8_t)(ta * 255);
                // "FeN" 大字（紫色）
                uint16_t titleCol = spr.color565(
                    (uint8_t)(tb * 0.66f),
                    (uint8_t)(tb * 0.25f),
                    tb);
                spr.setTextDatum(MC_DATUM);
                spr.setTextSize(2);
                spr.setTextColor(titleCol);
                spr.drawString("FeN", cx, 10);

                // "RC" 小字（白色，同行右侧）
                uint16_t rcCol = spr.color565(tb, tb, tb);
                spr.setTextSize(1);
                spr.setTextColor(rcCol);
                spr.drawString("RC", cx + 18, 10);
            }

            // ── Panbotica logo（最后阶段显示在底部）──
            if (ease > 0.75f) {
                uint8_t la = (uint8_t)((ease - 0.75f) / 0.25f * 255);
                // logo 居中
                int lx = (LCD_W - LOGO_W) / 2;
                int ly = LCD_H - LOGO_H - 2;
                for (int row = 0; row < LOGO_H; row++) {
                    for (int col = 0; col < LOGO_W; col++) {
                        uint16_t px = pgm_read_word(&PANBOTICA_LOGO[row][col]);
                        if (px != 0) {
                            // 与背景混合以实现淡入
                            uint8_t r = ((px >> 11) & 0x1F) * la / 255;
                            uint8_t g = ((px >> 5)  & 0x3F) * la / 255;
                            uint8_t b = ( px        & 0x1F) * la / 255;
                            spr.drawPixel(lx + col, ly + row,
                                spr.color565(r << 3, g << 2, b << 3));
                        }
                    }
                }
            }

            spr.pushSprite(0, 0);
            delay(42);  // ~24fps，总时长约 2 秒
        }

        spr.deleteSprite();

        // 动画结束，绘制静态 boot 画面等待后续步骤
        _drawBootStatic(0);
    }

    // 更新 boot 静态页面的连接状态（0=初始, 1=., 2=.., 3=...）
    void updateConnecting(int dots) {
        _drawBootStatic(dots);
    }

    // ── WiFi 滚动说明 ─────────────────────────────────────────────
    // 简洁上滚，textSize(2)，每行≤10字符，阅读速度舒适
    // 调用一次播放完整动画（约 6 秒）
    void showWifiScroll() {
        const char* lines[] = {
            "WiFi Setup",
            "",
            "1. Connect",
            "   WiFi:",
            "FeN-Ctrl",
            "pw:fenrobot1",
            "",
            "2. Open",
            "   browser:",
            "192.168.4.1",
            "",
            "3. Enter",
            "   settings",
            "   & save",
            "",
            "Waiting...",
        };
        const int N = 16;
        const int LINE_H = 18;       // textSize(2) 行高约 16px + 2px 间距
        const float SPEED = 0.5f;    // px/frame，舒适阅读速度

        LGFX_Sprite spr(&M5.Lcd);
        spr.createSprite(LCD_W, LCD_H);

        // 总滚动距离：所有行从屏幕底部滚出到顶部
        float totalScroll = (float)(N * LINE_H + LCD_H);
        float offset = 0;
        uint32_t lastFrame = millis();

        while (offset < totalScroll) {
            uint32_t now = millis();
            float dt = (float)(now - lastFrame) / 16.0f;
            lastFrame = now;
            offset += SPEED * dt;

            spr.fillScreen(C_BG);

            for (int i = 0; i < N; i++) {
                int screenY = (int)(LCD_H - offset + i * LINE_H);
                if (screenY < -LINE_H || screenY > LCD_H) continue;
                if (lines[i][0] == '\0') continue;

                // 颜色区分：标题行、高亮行、普通行
                uint16_t col;
                if (i == 0) {
                    col = C_YELLOW;
                } else if (strstr(lines[i], "FeN-Ctrl") || strstr(lines[i], "192.168")) {
                    col = C_ACCENT;
                } else if (lines[i][0] >= '1' && lines[i][0] <= '9' && lines[i][1] == '.') {
                    col = C_CYAN;
                } else {
                    col = C_WHITE;
                }

                spr.setTextSize(2);
                spr.setTextColor(col);
                spr.setTextDatum(MC_DATUM);
                spr.drawString(lines[i], LCD_W / 2, screenY + LINE_H / 2);
            }

            // 顶/底渐变遮罩
            for (int y = 0; y < 16; y++) {
                uint16_t mask = spr.color565(0, 0, 0);
                uint8_t a = (uint8_t)((16 - y) * 14);
                (void)a;  // 简单用纯黑线模拟渐变
                spr.drawFastHLine(0, y, LCD_W, mask);
            }
            for (int y = LCD_H - 16; y < LCD_H; y++) {
                spr.drawFastHLine(0, y, LCD_W, spr.color565(0, 0, 0));
            }

            spr.pushSprite(0, 0);
            delay(16);
        }

        spr.deleteSprite();
    }

    // WiFi 设置静态界面（动画结束后显示）
    void showWifiSetup() {
        M5.Lcd.fillScreen(TFT_BLACK);

        // 顶部标题栏
        M5.Lcd.fillRect(0, 0, LCD_W, 18, C_ACCENT);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(C_WHITE, C_ACCENT);
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString("WiFi Setup", LCD_W / 2, 9);

        // 连接提示
        M5.Lcd.setTextDatum(TL_DATUM);
        M5.Lcd.setTextSize(1);

        M5.Lcd.setTextColor(C_GRAY, TFT_BLACK);
        M5.Lcd.drawString("Connect WiFi:", 4, 24);
        M5.Lcd.setTextColor(C_WHITE, TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.drawString("FeN-Ctrl", 4, 34);

        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(C_GRAY, TFT_BLACK);
        M5.Lcd.drawString("Password:", 4, 54);
        M5.Lcd.setTextColor(C_GRAY, TFT_BLACK);
        M5.Lcd.drawString("fenrobot1", 4, 64);

        M5.Lcd.drawFastHLine(0, 76, LCD_W, C_DARK_GRAY);

        M5.Lcd.setTextColor(C_GRAY, TFT_BLACK);
        M5.Lcd.drawString("Open browser:", 4, 82);
        // IP 用 textSize(1) 确保不溢出，颜色高亮
        M5.Lcd.setTextColor(C_ACCENT, TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.drawString("192.168.4.1", 4, 92);

        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(C_YELLOW, TFT_BLACK);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString("Waiting...", LCD_W / 2, 116);
    }

    // ── 主界面：左右分栏显示双臂状态 ─────────────────────────────
    void showMain(CtrlMode mode, const StatusPacket& st,
                  int16_t lx, int16_t ly, int16_t rx, int16_t ry) {
        M5.Lcd.fillScreen(C_BG);

        // ── 顶部模式栏（全宽）──
        M5.Lcd.fillRect(0, 0, LCD_W, 14, C_ACCENT);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(C_WHITE, C_ACCENT);
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString(modeName(mode), LCD_W / 2, 7);

        // ── 连接状态指示点 ──
        uint16_t lc = (st.flags & 0x01) ? C_GREEN : C_RED;
        uint16_t rc = (st.flags & 0x02) ? C_GREEN : C_RED;
        M5.Lcd.fillCircle(4,  7, 3, lc);
        M5.Lcd.fillCircle(123, 7, 3, rc);

        // ── 中央分隔线 ──
        M5.Lcd.drawFastVLine(LCD_W / 2, 14, 96, C_DARK_GRAY);

        // ── 左臂（左半屏）──
        const int LX = 2;
        M5.Lcd.setTextDatum(TL_DATUM);
        M5.Lcd.setTextColor(C_CYAN, C_BG);
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString("LEFT", LX, 17);
        drawArmPanel(LX, 27, st.l_pos, st.l_state);

        // 左摇杆可视化
        drawMiniStick(14, 104, lx, ly, C_CYAN);
        // 左夹爪
        drawGripper(36, 104, st.l_gripper, C_CYAN);

        // ── 右臂（右半屏）──
        const int RX = LCD_W / 2 + 3;
        M5.Lcd.setTextColor(C_ORANGE, C_BG);
        M5.Lcd.drawString("RIGHT", RX, 17);
        drawArmPanel(RX, 27, st.r_pos, st.r_state);

        // 右摇杆可视化
        drawMiniStick(114, 104, rx, ry, C_ORANGE);
        // 右夹爪
        drawGripper(88, 104, st.r_gripper, C_ORANGE);

        // ── 底部分隔线 ──
        M5.Lcd.drawFastHLine(0, 116, LCD_W, C_DARK_GRAY);

        // ── 关节模式：显示当前关节编号 ──
        if (mode == MODE_JOINT) {
            char jbuf[12];
            snprintf(jbuf, 12, "J%d", (int)st.flags >> 4 & 0x0F);
            M5.Lcd.setTextDatum(MC_DATUM);
            M5.Lcd.setTextColor(C_YELLOW, C_BG);
            M5.Lcd.drawString(jbuf, LCD_W / 2, 122);
        }

        // ── 急停覆盖 ──
        if (mode == MODE_ESTOP) {
            M5.Lcd.fillRect(0, 48, LCD_W, 22, C_RED);
            M5.Lcd.setTextDatum(MC_DATUM);
            M5.Lcd.setTextColor(C_WHITE, C_RED);
            M5.Lcd.setTextSize(2);
            M5.Lcd.drawString("E-STOP", LCD_W / 2, 59);
        }
    }

    // ── 配置菜单 ──────────────────────────────────────────────────
    void showConfig(int item, const AppConfig& cfg) {
        M5.Lcd.fillScreen(C_BG);
        M5.Lcd.fillRect(0, 0, LCD_W, 14, 0x4010);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(C_YELLOW, 0x4010);
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString("CONFIG", LCD_W / 2, 7);

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
            int y = 17 + i * 11;
            if (y > 118) break;
            bool sel = (i == item);
            M5.Lcd.fillRect(0, y - 1, LCD_W, 11, sel ? C_ACCENT : C_BG);
            M5.Lcd.setTextDatum(TL_DATUM);
            M5.Lcd.setTextColor(sel ? C_WHITE : C_GRAY, sel ? C_ACCENT : C_BG);
            M5.Lcd.drawString(items[i], 6, y);
            M5.Lcd.setTextDatum(TR_DATUM);
            M5.Lcd.setTextColor(sel ? C_YELLOW : C_DARK_GRAY, sel ? C_ACCENT : C_BG);
            switch (i) {
                case 0: M5.Lcd.drawString(cfg.wifi_ssid[0] ? cfg.wifi_ssid : "?", 124, y); break;
                case 1: M5.Lcd.drawString(cfg.pc_ip, 124, y); break;
                case 2: M5.Lcd.drawString(cfg.left_arm_ip, 124, y); break;
                case 3: M5.Lcd.drawString(cfg.right_arm_ip, 124, y); break;
                case 4: { char b[8]; snprintf(b, 8, "%.0f", cfg.speed_cart);   M5.Lcd.drawString(b, 124, y); break; }
                case 5: { char b[8]; snprintf(b, 8, "%.0f", cfg.speed_orient); M5.Lcd.drawString(b, 124, y); break; }
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
        M5.Lcd.fillRect(2, 52, LCD_W - 4, 24, 0x2104);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(color, 0x2104);
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString(msg, LCD_W / 2, 64);
        if (durationMs > 0) delay(durationMs);
    }

private:
    // 静态 boot 画面（动画后持续显示，dots=0~3 控制 "..." 动画）
    void _drawBootStatic(int dots) {
        M5.Lcd.fillScreen(C_BG);

        // 标题
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(C_ACCENT, C_BG);
        M5.Lcd.setTextSize(2);
        M5.Lcd.drawString("FeN", 50, 10);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(C_WHITE, C_BG);
        M5.Lcd.drawString("RC", 79, 6);

        // 副标题
        M5.Lcd.setTextColor(C_GRAY, C_BG);
        M5.Lcd.drawString("Dual Arm Robot", LCD_W / 2, 24);

        // 机器人简化剪影（静态，与动画结束帧一致）
        const int cx = 64, bodyY = 72;
        uint16_t col = M5.Lcd.color565(110, 60, 180);
        M5.Lcd.fillRoundRect(cx - 8,  bodyY - 22, 16, 14, 3, col);
        M5.Lcd.fillRect     (cx - 3,  bodyY -  9,  6,  5,    col);
        M5.Lcd.fillRoundRect(cx - 12, bodyY -  4, 24, 20, 3, col);
        M5.Lcd.fillRect     (cx - 9,  bodyY + 16,  6, 10,    col);
        M5.Lcd.fillRect     (cx + 3,  bodyY + 16,  6, 10,    col);
        M5.Lcd.fillRect     (cx - 11, bodyY + 25,  9,  4,    col);
        M5.Lcd.fillRect     (cx + 2,  bodyY + 25,  9,  4,    col);
        // 眼睛
        M5.Lcd.fillCircle(cx - 3, bodyY - 16, 2, C_CYAN);
        M5.Lcd.fillCircle(cx + 3, bodyY - 16, 2, C_CYAN);
        // 双臂（展开后固定位置）
        M5.Lcd.drawLine(cx - 12, bodyY + 2, cx - 38, bodyY + 10, col);
        M5.Lcd.drawLine(cx - 38, bodyY + 10, cx - 46, bodyY + 16, col);
        M5.Lcd.fillCircle(cx - 46, bodyY + 16, 2, col);
        M5.Lcd.drawLine(cx + 12, bodyY + 2, cx + 38, bodyY + 10, col);
        M5.Lcd.drawLine(cx + 38, bodyY + 10, cx + 46, bodyY + 16, col);
        M5.Lcd.fillCircle(cx + 46, bodyY + 16, 2, col);

        // Panbotica logo
        int logoX = (LCD_W - LOGO_W) / 2;
        int logoY = LCD_H - LOGO_H - 2;
        for (int row = 0; row < LOGO_H; row++) {
            for (int c = 0; c < LOGO_W; c++) {
                uint16_t px = pgm_read_word(&PANBOTICA_LOGO[row][c]);
                if (px != 0) {
                    M5.Lcd.drawPixel(logoX + c, logoY + row, px);
                }
            }
        }

        // 连接中提示
        char buf[20];
        snprintf(buf, 20, "Connecting%.*s", dots, "...");
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(C_WHITE, C_BG);
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString(buf, LCD_W / 2, LCD_H - LOGO_H - 12);
    }

    // 单臂数据面板（半屏宽）
    void drawArmPanel(int x, int y, const float pos[3], uint8_t state) {
        char buf[10];
        uint16_t sc = (state == 1) ? C_YELLOW : (state == 2 ? C_RED : C_WHITE);
        M5.Lcd.setTextDatum(TL_DATUM);
        M5.Lcd.setTextSize(1);

        // X
        M5.Lcd.setTextColor(C_RED, C_BG);
        M5.Lcd.drawString("X", x, y);
        snprintf(buf, 10, "%+.0f", pos[0]);
        M5.Lcd.setTextColor(sc, C_BG);
        M5.Lcd.drawString(buf, x + 7, y);

        // Y
        M5.Lcd.setTextColor(C_GREEN, C_BG);
        M5.Lcd.drawString("Y", x, y + 11);
        snprintf(buf, 10, "%+.0f", pos[1]);
        M5.Lcd.setTextColor(sc, C_BG);
        M5.Lcd.drawString(buf, x + 7, y + 11);

        // Z
        M5.Lcd.setTextColor(0x001F, C_BG);  // 蓝色
        M5.Lcd.drawString("Z", x, y + 22);
        snprintf(buf, 10, "%+.0f", pos[2]);
        M5.Lcd.setTextColor(sc, C_BG);
        M5.Lcd.drawString(buf, x + 7, y + 22);
    }

    void drawMiniStick(int cx, int cy, int16_t vx, int16_t vy, uint16_t color) {
        const int R = 8;
        M5.Lcd.drawCircle(cx, cy, R, C_DARK_GRAY);
        int dx = map(vx, -1000, 1000, -(R - 2), R - 2);
        int dy = map(vy, -1000, 1000, -(R - 2), R - 2);
        M5.Lcd.fillCircle(cx + dx, cy + dy, 2, color);
    }

    void drawGripper(int x, int y, uint8_t pct, uint16_t color) {
        M5.Lcd.drawRect(x - 8, y - 4, 16, 8, C_DARK_GRAY);
        int w = map(pct, 0, 100, 0, 14);
        M5.Lcd.fillRect(x - 7, y - 3, w, 6, color);
    }

    const char* modeName(CtrlMode m) {
        switch (m) {
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
        switch (t) {
            case 0: return "xArm";
            case 1: return "BIO";
            case 2: return "Vacuum";
            default: return "?";
        }
    }
};
