#pragma once
#include <Preferences.h>
#include <Arduino.h>

// 默认值
#define DEFAULT_WIFI_SSID     ""
#define DEFAULT_WIFI_PASS     ""
#define DEFAULT_PC_IP         "192.168.1.100"
#define DEFAULT_LEFT_ARM_IP   "192.168.1.202"
#define DEFAULT_RIGHT_ARM_IP  "192.168.1.201"
#define DEFAULT_SPEED_CART    50.0f   // mm/s 笛卡尔平移速度
#define DEFAULT_SPEED_ORIENT  30.0f   // deg/s 姿态旋转速度
#define DEFAULT_SPEED_JOINT   20.0f   // deg/s 关节速度
#define DEFAULT_DEADZONE      60      // 摇杆死区 (out of 1000)
#define DEFAULT_GRIPPER_TYPE  0       // 0=xArm夹爪 1=BIO夹爪 2=真空吸盘

struct AppConfig {
    char  wifi_ssid[33];
    char  wifi_pass[65];
    char  pc_ip[16];
    char  left_arm_ip[16];
    char  right_arm_ip[16];
    float speed_cart;
    float speed_orient;
    float speed_joint;
    int   deadzone;
    int   gripper_type;
    bool  auto_discover;   // 是否开启局域网自动发现
};

class ConfigManager {
public:
    AppConfig cfg;

    void load() {
        Preferences prefs;
        prefs.begin("fen-ctrl", true);
        strlcpy(cfg.wifi_ssid,    prefs.getString("ssid",   DEFAULT_WIFI_SSID).c_str(),    sizeof(cfg.wifi_ssid));
        strlcpy(cfg.wifi_pass,    prefs.getString("pass",   DEFAULT_WIFI_PASS).c_str(),    sizeof(cfg.wifi_pass));
        strlcpy(cfg.pc_ip,        prefs.getString("pc_ip",  DEFAULT_PC_IP).c_str(),        sizeof(cfg.pc_ip));
        strlcpy(cfg.left_arm_ip,  prefs.getString("l_ip",   DEFAULT_LEFT_ARM_IP).c_str(),  sizeof(cfg.left_arm_ip));
        strlcpy(cfg.right_arm_ip, prefs.getString("r_ip",   DEFAULT_RIGHT_ARM_IP).c_str(), sizeof(cfg.right_arm_ip));
        cfg.speed_cart    = prefs.getFloat("sp_cart",   DEFAULT_SPEED_CART);
        cfg.speed_orient  = prefs.getFloat("sp_orient", DEFAULT_SPEED_ORIENT);
        cfg.speed_joint   = prefs.getFloat("sp_joint",  DEFAULT_SPEED_JOINT);
        cfg.deadzone      = prefs.getInt  ("deadzone",  DEFAULT_DEADZONE);
        cfg.gripper_type  = prefs.getInt  ("grip_type", DEFAULT_GRIPPER_TYPE);
        cfg.auto_discover = prefs.getBool ("auto_disc", true);
        prefs.end();
    }

    void save() {
        Preferences prefs;
        prefs.begin("fen-ctrl", false);
        prefs.putString("ssid",     cfg.wifi_ssid);
        prefs.putString("pass",     cfg.wifi_pass);
        prefs.putString("pc_ip",    cfg.pc_ip);
        prefs.putString("l_ip",     cfg.left_arm_ip);
        prefs.putString("r_ip",     cfg.right_arm_ip);
        prefs.putFloat ("sp_cart",  cfg.speed_cart);
        prefs.putFloat ("sp_orient",cfg.speed_orient);
        prefs.putFloat ("sp_joint", cfg.speed_joint);
        prefs.putInt   ("deadzone", cfg.deadzone);
        prefs.putInt   ("grip_type",cfg.gripper_type);
        prefs.putBool  ("auto_disc",cfg.auto_discover);
        prefs.end();
    }

    void reset() {
        Preferences prefs;
        prefs.begin("fen-ctrl", false);
        prefs.clear();
        prefs.end();
        load();
    }
};
