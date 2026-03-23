#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Arduino.h>
#include "Protocol.h"
#include "Config.h"

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
        _webConfigMode = true;
        _server = new WebServer(80);
        _server->on("/", [this]() { handleConfigRoot(); });
        _server->on("/save", HTTP_POST, [this]() { handleConfigSave(); });
        _server->begin();
    }

    void handleWebServer() {
        if (_webConfigMode && _server) _server->handleClient();
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
    WiFiUDP   _udp;
    WebServer* _server = nullptr;
    bool      _webConfigMode = false;
    bool      _pendingWebCfg = false;
    AppConfig _pendingConfig{};

    void handleConfigRoot() {
        String html = R"(<!DOCTYPE html><html><head>
<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>FeN Controller Setup</title>
<style>body{background:#111;color:#eee;font-family:monospace;padding:20px}
input{background:#222;color:#eee;border:1px solid #555;padding:6px;width:100%;margin:4px 0}
button{background:#4a2080;color:#fff;border:none;padding:10px 20px;cursor:pointer;margin-top:10px}
h2{color:#c8a0ff}label{color:#aaa;font-size:12px}</style></head>
<body><h2>FeN Robot Controller</h2>
<form method='post' action='/save'>
<label>WiFi SSID</label><input name='ssid' placeholder='your_wifi_ssid'>
<label>WiFi Password</label><input name='pass' type='password'>
<label>PC IP (ROS Bridge)</label><input name='pc_ip' value='192.168.1.100'>
<label>Left Arm IP</label><input name='l_ip' value='192.168.1.202'>
<label>Right Arm IP</label><input name='r_ip' value='192.168.1.201'>
<button type='submit'>Save & Reboot</button>
</form></body></html>)";
        _server->send(200, "text/html", html);
    }

    void handleConfigSave() {
        if (_server->hasArg("ssid")) {
            strlcpy(_pendingConfig.wifi_ssid, _server->arg("ssid").c_str(), 33);
            strlcpy(_pendingConfig.wifi_pass, _server->arg("pass").c_str(), 65);
            strlcpy(_pendingConfig.pc_ip,     _server->arg("pc_ip").c_str(), 16);
            strlcpy(_pendingConfig.left_arm_ip,  _server->arg("l_ip").c_str(), 16);
            strlcpy(_pendingConfig.right_arm_ip, _server->arg("r_ip").c_str(), 16);
            _pendingConfig.speed_cart   = DEFAULT_SPEED_CART;
            _pendingConfig.speed_orient = DEFAULT_SPEED_ORIENT;
            _pendingConfig.speed_joint  = DEFAULT_SPEED_JOINT;
            _pendingConfig.deadzone     = DEFAULT_DEADZONE;
            _pendingConfig.gripper_type = DEFAULT_GRIPPER_TYPE;
            _pendingConfig.auto_discover = true;
            _pendingWebCfg = true;
            _server->send(200, "text/html",
                "<html><body style='background:#111;color:#eee;font-family:monospace;padding:20px'>"
                "<h2 style='color:#c8a0ff'>Saved! Rebooting...</h2></body></html>");
        }
    }
};
