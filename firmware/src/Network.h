#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Arduino.h>
#include "Protocol.h"
#include "Config.h"

// iOS Captive Portal 检测 URL 列表（需要全部响应，否则 iOS 会认为网络异常）
static const char* CAPTIVE_URLS[] = {
    "/hotspot-detect.html",      // Apple
    "/library/test/success.html",// Apple
    "/generate_204",             // Android/Chrome
    "/connecttest.txt",          // Windows
    "/redirect",
    "/ncsi.txt",
};

class NetworkManager {
public:
    bool connected = false;
    StatusPacket lastStatus{};

    bool connectWifi(const AppConfig& cfg, int timeoutMs = 10000) {
        if (strlen(cfg.wifi_ssid) == 0) return false;
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
        uint32_t t = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - t) < (uint32_t)timeoutMs) {
            delay(200);
        }
        connected = (WiFi.status() == WL_CONNECTED);
        if (connected) {
            _udp.begin(UDP_PORT_STATUS);
        }
        return connected;
    }

    // 启动 AP 模式（用于首次配置 WiFi）
    void startAPMode(const char* apName = "FeN-Controller") {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apName, "fenrobot1");
        delay(100);  // 等待 AP 接口就绪

        // DNS 服务器：所有域名都解析到 192.168.4.1
        // 这样 iOS 的 captive portal 检测就能被我们捕获
        _dns = new DNSServer();
        _dns->setErrorReplyCode(DNSReplyCode::NoError);
        _dns->start(53, "*", WiFi.softAPIP());

        _webConfigMode = true;
        _server = new WebServer(80);

        // 注册配置页面
        _server->on("/",     HTTP_GET,  [this]() { handleConfigRoot(); });
        _server->on("/save", HTTP_POST, [this]() { handleConfigSave(); });

        // iOS captive portal 检测 —— 重定向到配置页
        for (auto& url : CAPTIVE_URLS) {
            _server->on(url, HTTP_GET, [this]() { redirectToConfig(); });
        }

        // 其他所有 URL 也重定向（兜底，确保 iOS 弹出浏览器）
        _server->onNotFound([this]() { redirectToConfig(); });

        _server->begin();
    }

    void handleWebServer() {
        if (!_webConfigMode) return;
        if (_dns)    _dns->processNextRequest();
        if (_server) _server->handleClient();
    }

    bool hasPendingWebConfig() { return _pendingWebCfg; }
    const AppConfig& pendingConfig() { return _pendingConfig; }
    void clearPendingWebConfig() { _pendingWebCfg = false; }

    // 发送控制包
    void sendCtrl(const char* pcIp, const CtrlPacket& pkt) {
        _udp.beginPacket(pcIp, UDP_PORT_CTRL);
        _udp.write((const uint8_t*)&pkt, sizeof(pkt));
        _udp.endPacket();
    }

    // 接收状态反馈（非阻塞）
    bool recvStatus() {
        int len = _udp.parsePacket();
        if (len < (int)sizeof(StatusPacket)) return false;
        StatusPacket tmp{};
        _udp.read((uint8_t*)&tmp, sizeof(tmp));
        if (tmp.hdr[0] != 0xFF || tmp.hdr[1] != 0xFF) return false;
        uint8_t cs = calcChecksum((uint8_t*)&tmp, sizeof(tmp) - 1);
        if (cs != tmp.checksum) return false;
        lastStatus = tmp;
        return true;
    }

    String localIP() {
        return WiFi.localIP().toString();
    }

private:
    WiFiUDP    _udp;
    WebServer* _server = nullptr;
    DNSServer* _dns    = nullptr;
    bool       _webConfigMode = false;
    bool       _pendingWebCfg = false;
    AppConfig  _pendingConfig{};

    // 重定向到配置首页（给 captive portal / 404 使用）
    void redirectToConfig() {
        _server->sendHeader("Location", "http://192.168.4.1/", true);
        _server->sendHeader("Cache-Control", "no-cache");
        _server->send(302, "text/plain", "");
    }

    void handleConfigRoot() {
        // 明确设置所有必要的 header，确保 iOS Safari / Chrome 正确渲染
        _server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        _server->sendHeader("Pragma", "no-cache");
        _server->sendHeader("Expires", "-1");
        _server->sendHeader("Connection", "close");

        // 分段构建 HTML，避免单一大字符串的潜在截断问题
        String html;
        html.reserve(1200);
        html += F("<!DOCTYPE html>\n<html>\n<head>\n");
        html += F("<meta charset='utf-8'>\n");
        html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>\n");
        html += F("<title>FeN Setup</title>\n");
        html += F("<style>\n");
        html += F("*{box-sizing:border-box}");
        html += F("body{background:#111;color:#eee;font-family:system-ui,monospace;");
        html += F("padding:24px;max-width:480px;margin:0 auto}");
        html += F("h2{color:#c8a0ff;margin-bottom:20px}");
        html += F("label{display:block;color:#aaa;font-size:13px;margin-top:14px;margin-bottom:4px}");
        html += F("input{background:#222;color:#eee;border:1px solid #555;");
        html += F("padding:10px;width:100%;border-radius:4px;font-size:16px}");
        html += F("button{background:#4a2080;color:#fff;border:none;padding:14px 24px;");
        html += F("border-radius:4px;font-size:16px;width:100%;margin-top:20px;cursor:pointer}");
        html += F("button:active{background:#6030a0}");
        html += F("</style>\n</head>\n<body>\n");
        html += F("<h2>FeN Robot Setup</h2>\n");
        html += F("<form method='post' action='/save'>\n");
        html += F("<label>WiFi SSID</label>");
        html += F("<input name='ssid' autocorrect='off' autocapitalize='none' placeholder='your_wifi_name'>\n");
        html += F("<label>WiFi Password</label>");
        html += F("<input name='pass' type='password' placeholder='password'>\n");
        html += F("<label>PC IP (ROS Bridge)</label>");
        html += F("<input name='pc_ip' value='192.168.1.100'>\n");
        html += F("<label>Left Arm IP</label>");
        html += F("<input name='l_ip' value='192.168.1.202'>\n");
        html += F("<label>Right Arm IP</label>");
        html += F("<input name='r_ip' value='192.168.1.201'>\n");
        html += F("<button type='submit'>Save &amp; Reboot</button>\n");
        html += F("</form>\n</body>\n</html>");

        _server->send(200, "text/html; charset=utf-8", html);
    }

    void handleConfigSave() {
        if (_server->hasArg("ssid") && _server->arg("ssid").length() > 0) {
            strlcpy(_pendingConfig.wifi_ssid, _server->arg("ssid").c_str(), 33);
            strlcpy(_pendingConfig.wifi_pass, _server->arg("pass").c_str(), 65);
            strlcpy(_pendingConfig.pc_ip,        _server->arg("pc_ip").c_str(), 16);
            strlcpy(_pendingConfig.left_arm_ip,  _server->arg("l_ip").c_str(),  16);
            strlcpy(_pendingConfig.right_arm_ip, _server->arg("r_ip").c_str(),  16);
            _pendingConfig.speed_cart    = DEFAULT_SPEED_CART;
            _pendingConfig.speed_orient  = DEFAULT_SPEED_ORIENT;
            _pendingConfig.speed_joint   = DEFAULT_SPEED_JOINT;
            _pendingConfig.deadzone      = DEFAULT_DEADZONE;
            _pendingConfig.gripper_type  = DEFAULT_GRIPPER_TYPE;
            _pendingConfig.auto_discover = true;
            _pendingWebCfg = true;

            String ok;
            ok += F("<!DOCTYPE html><html><head>");
            ok += F("<meta charset='utf-8'>");
            ok += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
            ok += F("</head><body style='background:#111;color:#eee;");
            ok += F("font-family:system-ui;padding:24px;text-align:center'>");
            ok += F("<h2 style='color:#c8a0ff'>Saved!</h2>");
            ok += F("<p>Device rebooting...<br>Rejoin your WiFi network.</p>");
            ok += F("</body></html>");
            _server->sendHeader("Cache-Control", "no-cache");
            _server->send(200, "text/html; charset=utf-8", ok);
        } else {
            redirectToConfig();
        }
    }
};
