"""
双臂 xArm 控制器
将手柄输入转换为机械臂运动指令
支持：xArm 6 / xArm 7（DOF 可配置）
"""
import logging
import threading
import time
from typing import Optional, Tuple

from xarm.wrapper import XArmAPI

from protocol import (
    CtrlPacket, StatusPacket,
    MODE_CART, MODE_CART_Z, MODE_ORIENT, MODE_JOINT, MODE_ESTOP,
)

logger = logging.getLogger("fen.arm")


class SingleArmController:
    """单臂控制封装"""

    def __init__(self, ip: str, dof: int = 6, tcp_offset=None, name: str = "arm"):
        self.ip   = ip
        self.dof  = dof
        self.name = name
        self.tcp_offset = tcp_offset or [0, 0, 0, 0, 0, 0]
        self.api: Optional[XArmAPI] = None
        self._lock = threading.Lock()
        self.connected = False
        self.state = 0       # 0=idle 1=moving 2=error
        self.pos   = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]  # x,y,z,roll,pitch,yaw
        self.gripper_pos = 0  # 0-850

    def connect(self) -> bool:
        try:
            logger.info(f"[{self.name}] Connecting to {self.ip}...")
            self.api = XArmAPI(self.ip, is_radian=False)
            self.api.motion_enable(True)
            self.api.clean_error()
            self.api.clean_warn()
            self.api.set_mode(0)    # 位置控制模式
            self.api.set_state(0)   # 运动状态
            # 设置 TCP 工具偏移
            if any(v != 0 for v in self.tcp_offset):
                self.api.set_tcp_offset(self.tcp_offset)
            self.connected = True
            self._update_pos()
            logger.info(f"[{self.name}] Connected! DOF={self.dof} pos={self.pos[:3]}")
            return True
        except Exception as e:
            logger.error(f"[{self.name}] Connect failed: {e}")
            self.connected = False
            return False

    def disconnect(self):
        if self.api:
            try:
                self.api.disconnect()
            except Exception:
                pass
        self.connected = False

    # ── 笛卡尔运动 ─────────────────────────────────────────────
    def move_cart_delta(self, dx: float, dy: float, dz: float,
                        droll: float = 0, dpitch: float = 0, dyaw: float = 0,
                        speed: float = 50):
        """相对末端位移（mm/deg）"""
        if not self.connected or self.api is None:
            return
        with self._lock:
            try:
                self.api.set_tool_position(
                    x=dx, y=dy, z=dz,
                    roll=droll, pitch=dpitch, yaw=dyaw,
                    speed=speed, is_radian=False, wait=False
                )
                self.state = 1
            except Exception as e:
                logger.warning(f"[{self.name}] move_cart_delta error: {e}")

    # ── 关节运动 ───────────────────────────────────────────────
    def move_joint_delta(self, joint_idx: int, delta_deg: float, speed: float = 20):
        """指定关节增量运动"""
        if not self.connected or self.api is None:
            return
        if joint_idx >= self.dof:
            return
        with self._lock:
            try:
                code, angles = self.api.get_servo_angle()
                if code != 0:
                    return
                angles[joint_idx] += delta_deg
                self.api.set_servo_angle(
                    angle=angles, speed=speed, wait=False
                )
                self.state = 1
            except Exception as e:
                logger.warning(f"[{self.name}] move_joint_delta error: {e}")

    # ── 夹爪控制 ───────────────────────────────────────────────
    def set_gripper(self, open_pct: float, cfg: dict):
        """
        open_pct: 0.0 = 完全闭合, 1.0 = 完全张开
        cfg: gripper 配置节
        """
        if not self.connected or self.api is None:
            return
        grip_type = cfg.get("type", "xarm")
        with self._lock:
            try:
                if grip_type == "xarm":
                    pos = int(cfg.get("close_pos", 0) +
                              open_pct * (cfg.get("open_pos", 800) - cfg.get("close_pos", 0)))
                    self.api.set_gripper_enable(True)
                    self.api.set_gripper_position(
                        pos,
                        speed=cfg.get("speed", 5000),
                        wait=False
                    )
                    self.gripper_pos = int(open_pct * 100)

                elif grip_type == "bio":
                    if open_pct > 0.5:
                        self.api.open_bio_gripper(speed=cfg.get("speed", 2000), wait=False)
                    else:
                        self.api.close_bio_gripper(speed=cfg.get("speed", 2000), wait=False)
                    self.gripper_pos = int(open_pct * 100)

                elif grip_type == "vacuum":
                    self.api.set_vacuum_gripper(open_pct > 0.5, wait=False)
                    self.gripper_pos = 100 if open_pct > 0.5 else 0

            except Exception as e:
                logger.warning(f"[{self.name}] gripper error: {e}")

    # ── 急停 ───────────────────────────────────────────────────
    def estop(self):
        if self.api:
            try:
                self.api.emergency_stop()
                self.state = 0
            except Exception:
                pass

    def resume(self):
        if self.api:
            try:
                self.api.clean_error()
                self.api.motion_enable(True)
                self.api.set_mode(0)
                self.api.set_state(0)
                self.state = 0
            except Exception:
                pass

    # ── 回零 ───────────────────────────────────────────────────
    def go_home(self, speed: float = 30):
        if not self.connected or self.api is None:
            return
        with self._lock:
            try:
                self.api.move_gohome(speed=speed, wait=False)
            except Exception as e:
                logger.warning(f"[{self.name}] go_home error: {e}")

    # ── 获取当前末端位姿 ────────────────────────────────────────
    def _update_pos(self):
        if self.api is None:
            return
        try:
            code, pos = self.api.get_position(is_radian=False)
            if code == 0 and pos:
                self.pos = list(pos)
        except Exception:
            pass

    def get_state(self) -> int:
        """0=idle 1=moving 2=error"""
        if not self.connected:
            return 2
        try:
            code, state = self.api.get_state()
            if code != 0:
                return 2
            return 1 if state == 1 else (2 if state in (3, 4) else 0)
        except Exception:
            return 2


class DualArmController:
    """双臂联合控制器"""

    def __init__(self, left_ip: str, right_ip: str, cfg: dict):
        self.cfg   = cfg
        self.left  = SingleArmController(left_ip,  cfg["arms"]["left"]["dof"],
                                         cfg["arms"]["left"]["tcp_offset"],  "LEFT")
        self.right = SingleArmController(right_ip, cfg["arms"]["right"]["dof"],
                                         cfg["arms"]["right"]["tcp_offset"], "RIGHT")
        self._estop_active = False
        self._prev_toggle  = False
        self._prev_front   = False
        self._status_lock  = threading.Lock()

    def connect_all(self):
        t_left  = threading.Thread(target=self.left.connect,  daemon=True)
        t_right = threading.Thread(target=self.right.connect, daemon=True)
        t_left.start()
        t_right.start()
        t_left.join(timeout=10)
        t_right.join(timeout=10)

    def disconnect_all(self):
        self.left.disconnect()
        self.right.disconnect()

    def process(self, pkt: CtrlPacket):
        """处理一帧手柄数据包"""
        ctrl = self.cfg["control"]
        grip = self.cfg["gripper"]
        dt   = ctrl["send_interval"]  # 每帧时间间隔

        mode = pkt.mode

        # 急停
        if mode == MODE_ESTOP:
            if not self._estop_active:
                logger.warning("E-STOP activated!")
                self.left.estop()
                self.right.estop()
                self._estop_active = True
            return
        elif self._estop_active:
            logger.info("E-STOP released, resuming...")
            self.left.resume()
            self.right.resume()
            self._estop_active = False

        # 摇杆归一化值
        lx, ly = pkt.lx_f, pkt.ly_f
        rx, ry = pkt.rx_f, pkt.ry_f

        # ── 笛卡尔 XY 模式（默认）──────────────────────────────
        if mode == MODE_CART:
            spd = ctrl["cart_speed"]
            dx_l = lx * spd * dt
            dy_l = ly * spd * dt
            dx_r = rx * spd * dt
            dy_r = ry * spd * dt
            if abs(dx_l) > 0.01 or abs(dy_l) > 0.01:
                self.left.move_cart_delta(dx_l, dy_l, 0, speed=spd)
            if abs(dx_r) > 0.01 or abs(dy_r) > 0.01:
                self.right.move_cart_delta(dx_r, dy_r, 0, speed=spd)

        # ── Z 轴模式（按住摇杆）──────────────────────────────────
        elif mode == MODE_CART_Z:
            spd = ctrl["cart_speed"]
            # 左摇杆 click → 左臂 Z，右摇杆 click → 右臂 Z
            if pkt.l_click:
                dz = ly * spd * dt
                if abs(dz) > 0.01:
                    self.left.move_cart_delta(0, 0, dz, speed=spd)
            if pkt.r_click:
                dz = ry * spd * dt
                if abs(dz) > 0.01:
                    self.right.move_cart_delta(0, 0, dz, speed=spd)
            # 另一轴继续 XY
            if not pkt.l_click:
                dx = lx * spd * dt
                dy = ly * spd * dt  # 注意：Z 模式时 ly 用于 Z
                if abs(dx) > 0.01:
                    self.left.move_cart_delta(dx, 0, 0, speed=spd)
            if not pkt.r_click:
                dx = rx * spd * dt
                if abs(dx) > 0.01:
                    self.right.move_cart_delta(dx, 0, 0, speed=spd)

        # ── 姿态模式（按住 BTN_A）────────────────────────────────
        elif mode == MODE_ORIENT:
            spd = ctrl["orient_speed"]
            dr_l  = ly * spd * dt   # 左摇杆 Y → Roll
            dp_l  = lx * spd * dt   # 左摇杆 X → Pitch
            dr_r  = ry * spd * dt
            dp_r  = rx * spd * dt
            if abs(dr_l) > 0.01 or abs(dp_l) > 0.01:
                self.left.move_cart_delta(0, 0, 0, dr_l, dp_l, 0, speed=spd)
            if abs(dr_r) > 0.01 or abs(dp_r) > 0.01:
                self.right.move_cart_delta(0, 0, 0, dr_r, dp_r, 0, speed=spd)

        # ── 关节模式（按住 BTN_B）────────────────────────────────
        elif mode == MODE_JOINT:
            spd   = ctrl["joint_speed"]
            j_sel = pkt.joint_sel
            # joint_sel: 0-6 = 左臂关节 1-7, 8-14 = 右臂关节 1-7
            if j_sel < 8:
                delta = ry * spd * dt  # 右摇杆 Y 调整关节
                if abs(delta) > 0.1:
                    self.left.move_joint_delta(j_sel, delta, speed=spd)
            else:
                delta = ry * spd * dt
                if abs(delta) > 0.1:
                    self.right.move_joint_delta(j_sel - 8, delta, speed=spd)

        # ── 夹爪控制（拨杆）─────────────────────────────────────
        toggle = pkt.toggle
        if toggle != self._prev_toggle:
            open_pct = 1.0 if toggle else 0.0
            self.left.set_gripper(open_pct, grip)
            self.right.set_gripper(open_pct, grip)
            self._prev_toggle = toggle

    def build_status(self) -> StatusPacket:
        """构建状态反馈包"""
        st = StatusPacket()
        self.left._update_pos()
        self.right._update_pos()

        st.flags = 0
        if self.left.connected:  st.flags |= 0x01
        if self.right.connected: st.flags |= 0x02
        if self._estop_active:   st.flags |= 0x04

        l_err = self.left.get_state()
        r_err = self.right.get_state()
        if l_err == 2 or r_err == 2: st.flags |= 0x08

        st.l_pos    = self.left.pos[:3]  if len(self.left.pos)  >= 3 else [0,0,0]
        st.r_pos    = self.right.pos[:3] if len(self.right.pos) >= 3 else [0,0,0]
        st.l_state  = l_err
        st.r_state  = r_err
        st.l_gripper = self.left.gripper_pos
        st.r_gripper = self.right.gripper_pos
        return st
