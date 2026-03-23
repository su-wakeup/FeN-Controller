"""
FeN Controller UDP 通信协议
与固件 Protocol.h 完全对应
"""
import struct
from dataclasses import dataclass, field
from typing import Optional

# UDP 端口
UDP_PORT_CTRL   = 9001   # ESP32 → PC
UDP_PORT_STATUS = 9002   # PC → ESP32

# 控制模式
MODE_CART    = 0
MODE_CART_Z  = 1
MODE_ORIENT  = 2
MODE_JOINT   = 3
MODE_CONFIG  = 4
MODE_ESTOP   = 5

MODE_NAMES = {
    MODE_CART:   "CART_XY",
    MODE_CART_Z: "CART_Z",
    MODE_ORIENT: "ORIENT",
    MODE_JOINT:  "JOINT",
    MODE_CONFIG: "CONFIG",
    MODE_ESTOP:  "E-STOP",
}

# 按键位掩码
BTN_L_CLICK = (1 << 0)
BTN_R_CLICK = (1 << 1)
BTN_A       = (1 << 2)
BTN_B       = (1 << 3)
BTN_TOGGLE  = (1 << 4)
BTN_FRONT   = (1 << 5)

# 手柄数据包格式（15 字节）
#  2B header + 1B seq + 4×2B axes + 1B buttons + 1B mode + 1B joint_sel + 1B checksum
CTRL_FMT  = "<2BbhhhhBBBB"   # 注意 hdr 是 2B, seq 是 1B uint8
CTRL_SIZE = struct.calcsize(CTRL_FMT)

# 状态反馈包格式（32 字节）
#  2B header + 1B flags + 6×4B floats + 1B l_state + 1B r_state + 1B l_grip + 1B r_grip + 1B battery + 1B checksum
STATUS_FMT  = "<2sBffffffBBBBBB"
STATUS_SIZE = struct.calcsize(STATUS_FMT)


@dataclass
class CtrlPacket:
    seq:       int = 0
    lx:        int = 0   # -1000 ~ 1000
    ly:        int = 0
    rx:        int = 0
    ry:        int = 0
    buttons:   int = 0
    mode:      int = MODE_CART
    joint_sel: int = 0

    @property
    def l_click(self): return bool(self.buttons & BTN_L_CLICK)
    @property
    def r_click(self): return bool(self.buttons & BTN_R_CLICK)
    @property
    def btn_a(self):   return bool(self.buttons & BTN_A)
    @property
    def btn_b(self):   return bool(self.buttons & BTN_B)
    @property
    def toggle(self):  return bool(self.buttons & BTN_TOGGLE)
    @property
    def front(self):   return bool(self.buttons & BTN_FRONT)

    # 归一化摇杆值 (-1.0 ~ 1.0)
    @property
    def lx_f(self): return self.lx / 1000.0
    @property
    def ly_f(self): return self.ly / 1000.0
    @property
    def rx_f(self): return self.rx / 1000.0
    @property
    def ry_f(self): return self.ry / 1000.0

    @staticmethod
    def parse(data: bytes) -> Optional["CtrlPacket"]:
        if len(data) < 15:
            return None
        if data[0] != 0xFE or data[1] != 0xFE:
            return None
        cs = 0
        for b in data[:14]:
            cs ^= b
        if cs != data[14]:
            return None
        # 手动解析（避免 struct unpack 对 signed/unsigned 混合的问题）
        pkt = CtrlPacket()
        pkt.seq       = data[2]
        pkt.lx        = struct.unpack_from("<h", data, 3)[0]
        pkt.ly        = struct.unpack_from("<h", data, 5)[0]
        pkt.rx        = struct.unpack_from("<h", data, 7)[0]
        pkt.ry        = struct.unpack_from("<h", data, 9)[0]
        pkt.buttons   = data[11]
        pkt.mode      = data[12]
        pkt.joint_sel = data[13]
        return pkt

    def __repr__(self):
        return (f"CtrlPacket(seq={self.seq} mode={MODE_NAMES.get(self.mode,'?')} "
                f"L=({self.lx},{self.ly}) R=({self.rx},{self.ry}) "
                f"btns=0b{self.buttons:06b})")


@dataclass
class StatusPacket:
    flags:      int   = 0      # bit0=左连 bit1=右连 bit2=急停 bit3=错误
    l_pos:      list  = field(default_factory=lambda: [0.0, 0.0, 0.0])
    r_pos:      list  = field(default_factory=lambda: [0.0, 0.0, 0.0])
    l_state:    int   = 0      # 0=idle 1=moving 2=error
    r_state:    int   = 0
    l_gripper:  int   = 0      # 0-100%
    r_gripper:  int   = 0
    battery:    int   = 100

    @property
    def left_connected(self):  return bool(self.flags & 0x01)
    @property
    def right_connected(self): return bool(self.flags & 0x02)
    @property
    def estop(self):           return bool(self.flags & 0x04)
    @property
    def error(self):           return bool(self.flags & 0x08)

    def pack(self) -> bytes:
        data = bytearray(32)
        data[0] = 0xFF
        data[1] = 0xFF
        data[2] = self.flags
        struct.pack_into("<fff", data, 3,  *self.l_pos)
        struct.pack_into("<fff", data, 15, *self.r_pos)
        data[27] = self.l_state
        data[28] = self.r_state
        data[29] = self.l_gripper
        data[30] = self.battery
        cs = 0
        for b in data[:31]:
            cs ^= b
        data[31] = cs
        return bytes(data)
