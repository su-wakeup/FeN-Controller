#include <M5Unified.h>
#include <WiFi.h>
#include "Protocol.h"
#include "Config.h"
#include "JoyStick.h"
#include "Display.h"
#include "Network.h"

// ─── 全局对象 ─────────────────────────────────────────────────
ConfigManager  cfgMgr;
JoyStickDriver joy;
DisplayManager disp;
NetworkManager net;

// ─── 状态变量 ─────────────────────────────────────────────────
CtrlMode  mode      = MODE_CART;
uint8_t   seq       = 0;
uint8_t   jointSel  = 0;   // 0-6 左臂关节，8-14 右臂关节
bool      prevFront = false;
uint32_t  frontPressTime = 0;
bool      prevBtnA  = false;
bool      prevBtnB  = false;
uint32_t  lastSendMs   = 0;
uint32_t  lastDisplayMs = 0;
int       configItem = 0;
bool      inConfigMenu = false;

// ─── 前置声明 ─────────────────────────────────────────────────
void handleConfigMenu(const JoyState& js);
void buildAndSend(const JoyState& js);
uint8_t buildButtons(const JoyState& js, bool frontBtn);
CtrlMode resolveMode(const JoyState& js, bool frontBtn);
void adjustConfigItem(int item, int dir);

// ─── 初始化 ──────────────────────────────────────────────────
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Lcd.setRotation(0);
    Serial.begin(115200);

    cfgMgr.load();
    disp.begin();

    // 初始化摇杆（I2C 地址 0x59，重试 3 次）
    bool joyOk = false;
    for (int attempt = 0; attempt < 3 && !joyOk; attempt++) {
        joyOk = joy.begin();
        if (!joyOk) delay(300);
    }
    if (!joyOk) {
        disp.showMessage("I2C Err 0x59", C_RED, 0);  // 0=不自动消失
        delay(2000);
    } else {
        delay(200);
        joy.calibrate();
    }

    // 连接 WiFi（有动画更新连接点）
    if (strlen(cfgMgr.cfg.wifi_ssid) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfgMgr.cfg.wifi_ssid, cfgMgr.cfg.wifi_pass);
        uint32_t t0 = millis();
        int dots = 0;
        while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 8000) {
            disp.updateConnecting(dots % 4);
            dots++;
            delay(500);
        }
    }
    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    if (wifiOk) net.markConnected();

    if (!wifiOk) {
        // 无 WiFi 配置 → AP 模式 + 网页配置
        disp.showWifiScroll();
        disp.showWifiSetup();
        net.startAPMode("FeN-Ctrl");
        // 等待用户通过网页配置后重启
        while (true) {
            net.handleWebServer();
            if (net.hasPendingWebConfig()) {
                cfgMgr.cfg = net.pendingConfig();
                cfgMgr.save();
                delay(500);
                ESP.restart();
            }
            M5.update();
        }
    }

    disp.showMessage("WiFi OK!", C_GREEN, 800);
    Serial.printf("[WiFi] IP: %s\n", net.localIP().c_str());
}

// ─── 主循环 ──────────────────────────────────────────────────
void loop() {
    M5.update();

    JoyState js = joy.read();
    bool frontBtn = M5.BtnA.isPressed();  // AtomS3 正面按键

    // ── 正面键逻辑：短按急停，长按>1.5s 进配置 ──────────────
    if (frontBtn && !prevFront) {
        frontPressTime = millis();
    }
    if (!frontBtn && prevFront) {
        uint32_t held = millis() - frontPressTime;
        if (held < 400) {
            // 短按：急停切换
            if (mode == MODE_ESTOP) {
                mode = MODE_CART;
                disp.showMessage("RESUMED", C_GREEN, 600);
            } else {
                mode = MODE_ESTOP;
            }
        }
        // 长按在下面持续检测
    }
    if (frontBtn && (millis() - frontPressTime) > 1500 && !inConfigMenu) {
        inConfigMenu = true;
        configItem   = 0;
        mode         = MODE_CONFIG;
    }
    prevFront = frontBtn;

    // ── 配置菜单 ──────────────────────────────────────────────
    if (inConfigMenu) {
        handleConfigMenu(js);
        return;
    }

    // ── 解析当前控制模式 ─────────────────────────────────────
    if (mode != MODE_ESTOP) {
        mode = resolveMode(js, frontBtn);
    }

    // ── 发送控制包（50Hz）────────────────────────────────────
    if (millis() - lastSendMs >= 20) {
        lastSendMs = millis();
        buildAndSend(js);
    }

    // ── 接收状态反馈 ─────────────────────────────────────────
    net.recvStatus();

    // ── 刷新显示（10Hz）─────────────────────────────────────
    if (millis() - lastDisplayMs >= 100) {
        lastDisplayMs = millis();
        disp.showMain(mode, net.lastStatus, js.lx, js.ly, js.rx, js.ry);
    }
}

// ─── 解析模式 ─────────────────────────────────────────────────
CtrlMode resolveMode(const JoyState& js, bool frontBtn) {
    if (js.btn_a && js.btn_b) return MODE_CART;  // A+B 一起按：回默认
    if (js.btn_b)   return MODE_JOINT;
    if (js.btn_a)   return MODE_ORIENT;
    if (js.l_btn || js.r_btn) return MODE_CART_Z;
    return MODE_CART;
}

// ─── 构建并发送控制包 ─────────────────────────────────────────
void buildAndSend(const JoyState& js) {
    const AppConfig& cfg = cfgMgr.cfg;

    // 死区处理
    auto dz = [&](int16_t v) -> int16_t {
        return abs(v) < cfg.deadzone ? 0 : v;
    };

    CtrlPacket pkt{};
    pkt.hdr[0] = 0xFE;
    pkt.hdr[1] = 0xFE;
    pkt.seq    = seq++;
    pkt.lx     = dz(js.lx);
    pkt.ly     = dz(js.ly);
    pkt.rx     = dz(js.rx);
    pkt.ry     = dz(js.ry);
    pkt.mode   = (uint8_t)mode;
    pkt.joint_sel = jointSel;

    // 按键位掩码
    pkt.buttons = 0;
    if (js.l_btn) pkt.buttons |= BTN_L_CLICK;
    if (js.r_btn) pkt.buttons |= BTN_R_CLICK;
    if (js.btn_a) pkt.buttons |= BTN_A;
    if (js.btn_b) pkt.buttons |= BTN_B;
    // 拨杆：AtomS3 没有专用拨杆 GPIO 读取，通过 I2C 的 BTN 口扩展
    // 此处预留，实际根据硬件接线确认
    if (M5.BtnA.isPressed()) pkt.buttons |= BTN_FRONT;

    pkt.checksum = calcChecksum((uint8_t*)&pkt, sizeof(pkt) - 1);

    if (net.connected) {
        net.sendCtrl(cfg.pc_ip, pkt);
    }

    // 关节模式：左摇杆 Y 滚动选关节
    if (mode == MODE_JOINT) {
        static uint32_t lastJointScroll = 0;
        if (millis() - lastJointScroll > 300) {
            int16_t ly = dz(js.ly);
            if (ly > 500) {
                jointSel = (jointSel + 1) % 14;
                lastJointScroll = millis();
            } else if (ly < -500) {
                jointSel = (jointSel == 0) ? 13 : jointSel - 1;
                lastJointScroll = millis();
            }
        }
    }
}

// ─── 配置菜单交互 ─────────────────────────────────────────────
void handleConfigMenu(const JoyState& js) {
    const int ITEMS = 10;
    static bool prevLBtn = false;
    static bool prevRBtn = false;
    static uint32_t lastScroll = 0;

    disp.showConfig(configItem, cfgMgr.cfg);

    // 左摇杆 Y：滚动菜单
    if (millis() - lastScroll > 250) {
        if (js.ly > 500) {
            configItem = (configItem + 1) % ITEMS;
            lastScroll = millis();
        } else if (js.ly < -500) {
            configItem = (configItem == 0) ? ITEMS-1 : configItem - 1;
            lastScroll = millis();
        }
    }

    // 左摇杆 X：调整数值
    if (millis() - lastScroll > 200) {
        if (abs(js.lx) > 500) {
            adjustConfigItem(configItem, js.lx > 0 ? 1 : -1);
            lastScroll = millis();
        }
    }

    // 左摇杆按下：确认选中
    if (js.l_btn && !prevLBtn) {
        if (configItem == 9) {  // Exit
            cfgMgr.save();
            inConfigMenu = false;
            mode = MODE_CART;
            disp.showMessage("Config saved", C_GREEN, 800);
        } else if (configItem == 8) {  // Factory Reset
            cfgMgr.reset();
            disp.showMessage("Reset! Reboot", C_RED, 1500);
            ESP.restart();
        }
    }
    prevLBtn = js.l_btn;

    // 正面键退出配置
    if (M5.BtnA.wasReleased()) {
        cfgMgr.save();
        inConfigMenu = false;
        mode = MODE_CART;
    }

    M5.update();
}

void adjustConfigItem(int item, int dir) {
    AppConfig& cfg = cfgMgr.cfg;
    switch (item) {
        case 4: cfg.speed_cart   = constrain(cfg.speed_cart   + dir * 5, 5, 200); break;
        case 5: cfg.speed_orient = constrain(cfg.speed_orient + dir * 5, 5, 100); break;
        case 6: cfg.gripper_type = (cfg.gripper_type + dir + 3) % 3; break;
        case 7: cfg.auto_discover = !cfg.auto_discover; break;
        // IP 和 SSID 编辑需要全键盘，此处留给网页配置
        default: break;
    }
}
