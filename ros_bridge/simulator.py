#!/usr/bin/env python3
"""
FeN Robot PC Simulator
接收手柄 UDP 控制包，在终端显示虚拟双臂状态，并回送状态反馈包。
无需 ROS，无需真实机械臂，用于验证手柄↔PC 数据链路。

用法:
    python3 simulator.py [--ip 0.0.0.0] [--verbose]
"""

import socket
import struct
import time
import math
import argparse
import threading
import os
import sys

from protocol import (
    CtrlPacket, StatusPacket,
    UDP_PORT_CTRL, UDP_PORT_STATUS,
    MODE_NAMES, MODE_ESTOP, MODE_CART, MODE_CART_Z, MODE_ORIENT, MODE_JOINT,
    BTN_A, BTN_B, BTN_L_CLICK, BTN_R_CLICK, BTN_FRONT,
)

# ── 虚拟机器人状态 ───────────────────────────────────────────────
class VirtualArm:
    def __init__(self, name: str, color: str):
        self.name   = name
        self.color  = color
        self.pos    = [300.0, 0.0, 200.0]   # X Y Z mm
        self.gripper = 50                    # 0-100%
        self.state  = 0                      # 0=idle 1=moving 2=error

    def apply_cart(self, dx: float, dy: float, speed: float):
        self.pos[0] = max(-600, min(600, self.pos[0] + dx * speed * 0.02))
        self.pos[1] = max(-600, min(600, self.pos[1] + dy * speed * 0.02))
        self.state  = 1 if (abs(dx) > 0.05 or abs(dy) > 0.05) else 0

    def apply_cart_z(self, dz: float, speed: float):
        self.pos[2] = max(0, min(600, self.pos[2] + dz * speed * 0.02))
        self.state  = 1 if abs(dz) > 0.05 else 0

    def apply_gripper(self, delta: float):
        self.gripper = max(0, min(100, self.gripper + delta))


# ── ANSI 颜色 ────────────────────────────────────────────────────
R  = "\033[91m"   # red
G  = "\033[92m"   # green
Y  = "\033[93m"   # yellow
B  = "\033[94m"   # blue
M  = "\033[95m"   # magenta
C  = "\033[96m"   # cyan
W  = "\033[97m"   # white
DIM = "\033[2m"
RST = "\033[0m"
CLEAR = "\033[2J\033[H"

def bar(val: float, maxval: float, width: int = 20, color: str = G) -> str:
    filled = int(val / maxval * width)
    filled = max(0, min(width, filled))
    return color + "█" * filled + DIM + "░" * (width - filled) + RST

def gripper_bar(pct: int, color: str) -> str:
    return bar(pct, 100, 14, color)


# ── 渲染 ─────────────────────────────────────────────────────────
def render(left: VirtualArm, right: VirtualArm, pkt: CtrlPacket,
           mode_str: str, estop: bool, fps: float, pkt_count: int):
    lines = []
    lines.append(CLEAR)
    lines.append(f"{M}{'═'*60}{RST}")
    lines.append(f"  {M}FeN{RST} {W}Robot Simulator{RST}   "
                 f"{DIM}pkts:{pkt_count}  {fps:.0f}Hz{RST}")
    lines.append(f"{M}{'═'*60}{RST}")

    # 模式栏
    mode_color = R if estop else Y
    lines.append(f"  Mode: {mode_color}{mode_str:14s}{RST}  "
                 f"seq={pkt.seq:3d}  "
                 f"btns=0b{pkt.buttons:06b}")

    if estop:
        lines.append(f"\n  {R}{'!! E-STOP !! 急停中 !!':^40s}{RST}\n")
    else:
        lines.append("")

    # 双臂并排
    lc, rc = C, Y
    lstate = ["idle", "moving", "error"][left.state]
    rstate = ["idle", "moving", "error"][right.state]

    lines.append(f"  {lc}LEFT ARM{RST}{'':20s}{rc}RIGHT ARM{RST}")
    lines.append(f"  {lc}X{RST} {left.pos[0]:+7.1f} mm      "
                 f"  {rc}X{RST} {right.pos[0]:+7.1f} mm")
    lines.append(f"  {lc}Y{RST} {left.pos[1]:+7.1f} mm      "
                 f"  {rc}Y{RST} {right.pos[1]:+7.1f} mm")
    lines.append(f"  {lc}Z{RST} {left.pos[2]:+7.1f} mm      "
                 f"  {rc}Z{RST} {right.pos[2]:+7.1f} mm")
    lines.append(f"  state: {lc}{lstate}{RST}            "
                 f"  state: {rc}{rstate}{RST}")

    # 夹爪
    lines.append(f"\n  {lc}Gripper:{RST} {gripper_bar(left.gripper,  lc)} {left.gripper:3d}%"
                 f"   {rc}Gripper:{RST} {gripper_bar(right.gripper, rc)} {right.gripper:3d}%")

    # 摇杆可视化（ASCII 十字）
    def stick_vis(vx, vy, color):
        SZ = 5
        cx = cy = SZ
        dx = int(vx / 1000 * SZ)
        dy = int(vy / 1000 * SZ)
        rows = []
        for row in range(SZ * 2 + 1):
            line_str = ""
            for col in range(SZ * 2 + 1):
                if row == cy + dy and col == cx + dx:
                    line_str += color + "●" + RST
                elif row == cy and col == cx:
                    line_str += DIM + "+" + RST
                elif row == cy:
                    line_str += DIM + "─" + RST
                elif col == cx:
                    line_str += DIM + "│" + RST
                else:
                    line_str += " "
            rows.append(line_str)
        return rows

    lrows = stick_vis(pkt.lx, pkt.ly, lc)
    rrows = stick_vis(pkt.rx, pkt.ry, rc)
    lines.append("")
    lines.append(f"  {lc}L-Stick{RST} ({pkt.lx:+5d},{pkt.ly:+5d})   "
                 f"  {rc}R-Stick{RST} ({pkt.rx:+5d},{pkt.ry:+5d})")
    for i, (lr, rr) in enumerate(zip(lrows, rrows)):
        lines.append(f"  {lr}         {rr}")

    lines.append(f"\n{DIM}  Ctrl+C to quit{RST}")
    print("\n".join(lines), end="", flush=True)


# ── 主逻辑 ───────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="FeN Robot Simulator")
    parser.add_argument("--ip",      default="0.0.0.0", help="Listen IP")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    # 接收控制包
    recv_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    recv_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    recv_sock.bind((args.ip, UDP_PORT_CTRL))
    recv_sock.settimeout(0.1)

    # 发送状态包
    send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    left  = VirtualArm("LEFT",  "cyan")
    right = VirtualArm("RIGHT", "yellow")

    last_pkt    = CtrlPacket()
    pkt_count   = 0
    last_fps_t  = time.time()
    fps         = 0.0
    remote_addr = None
    status_seq  = 0
    last_status_t = time.time()
    speed = 50.0
    prev_l_click = False   # 边沿检测
    prev_r_click = False

    print(f"{G}FeN Simulator started{RST}  "
          f"listening UDP :{UDP_PORT_CTRL}  "
          f"status → :{UDP_PORT_STATUS}")
    print(f"{DIM}Waiting for gamepad packets...{RST}\n")

    try:
        while True:
            # ── 接收包：排空缓冲区，对每包做物理更新 ──
            pkts_this_frame = []
            while True:
                try:
                    data, addr = recv_sock.recvfrom(64)
                    pkt = CtrlPacket.parse(data)
                    if pkt:
                        pkts_this_frame.append((pkt, addr))
                        pkt_count += 1
                except socket.timeout:
                    break

            for pkt, addr in pkts_this_frame:
                last_pkt    = pkt
                remote_addr = (addr[0], UDP_PORT_STATUS)

                estop = (pkt.mode == MODE_ESTOP)
                if not estop:
                    if pkt.mode == MODE_CART:
                        left.apply_cart(  pkt.lx_f,  pkt.ly_f, speed)
                        right.apply_cart( pkt.rx_f,  pkt.ry_f, speed)
                    elif pkt.mode == MODE_CART_Z:
                        left.apply_cart_z( pkt.ly_f, speed)
                        right.apply_cart_z(pkt.ry_f, speed)
                    elif pkt.mode == MODE_ORIENT:
                        left.pos[1]  = max(-600, min(600, left.pos[1]  + pkt.lx_f * speed * 0.02))
                        right.pos[1] = max(-600, min(600, right.pos[1] + pkt.rx_f * speed * 0.02))

                    # 夹爪：边沿检测，避免噪声反复触发
                    if pkt.l_click and not prev_l_click:
                        left.gripper  = 0 if left.gripper  > 50 else 100
                    if pkt.r_click and not prev_r_click:
                        right.gripper = 0 if right.gripper > 50 else 100
                    prev_l_click = pkt.l_click
                    prev_r_click = pkt.r_click
                else:
                    left.state  = 0
                    right.state = 0

            # fps 统计
            now = time.time()
            if now - last_fps_t >= 1.0:
                fps = pkt_count / (now - last_fps_t + 1e-9)
                pkt_count  = 0
                last_fps_t = now

            if args.verbose and pkts_this_frame:
                print(last_pkt)

            estop = (last_pkt.mode == MODE_ESTOP)

            # ── 回送状态包（10Hz）──
            now = time.time()
            if remote_addr and (now - last_status_t) >= 0.1:
                last_status_t = now
                st = StatusPacket()
                st.flags     = (0x01 | 0x02)   # 左右臂均"连接"
                if estop: st.flags |= 0x04
                st.l_pos     = list(left.pos)
                st.r_pos     = list(right.pos)
                st.l_state   = left.state
                st.r_state   = right.state
                st.l_gripper = left.gripper
                st.r_gripper = right.gripper
                st.battery   = 85
                send_sock.sendto(st.pack(), remote_addr)

            # ── 渲染（~10Hz）──
            render(left, right, last_pkt,
                   MODE_NAMES.get(last_pkt.mode, "?"),
                   estop, fps, pkt_count)

            time.sleep(0.1)

    except KeyboardInterrupt:
        print(f"\n{Y}Simulator stopped.{RST}")


if __name__ == "__main__":
    main()
