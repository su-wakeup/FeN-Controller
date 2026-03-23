#!/usr/bin/env python3
"""
FeN Robot Controller - ROS Bridge 主程序

用法:
    python3 fen_bridge.py [--config config/config.yaml] [--ros-version auto|ros1|ros2|none]

自动检测 ROS 环境，支持 ROS1 Noetic、ROS2 Humble/Iron 以及纯 Python 模式。
"""

import argparse
import logging
import os
import socket
import sys
import threading
import time
from typing import Optional

import yaml

from protocol import CtrlPacket, StatusPacket, UDP_PORT_CTRL, UDP_PORT_STATUS
from arm_controller import DualArmController
from discovery import discover_xarms, assign_arms

# ─── 日志配置 ──────────────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("fen.bridge")


# ─── ROS 适配层 ─────────────────────────────────────────────────
def _detect_ros_version() -> str:
    """自动检测可用的 ROS 版本"""
    try:
        import rclpy
        return "ros2"
    except ImportError:
        pass
    try:
        import rospy
        return "ros1"
    except ImportError:
        pass
    return "none"


class RosPublisher:
    """ROS1/ROS2 状态发布（可选）"""

    def __init__(self, version: str, ns: str):
        self.version = version
        self.ns = ns
        self._node = None

        if version == "ros2":
            self._init_ros2(ns)
        elif version == "ros1":
            self._init_ros1(ns)
        else:
            logger.info("Running without ROS (standalone mode)")

    def _init_ros2(self, ns):
        try:
            import rclpy
            from rclpy.node import Node
            from geometry_msgs.msg import PoseStamped
            from std_msgs.msg import Int32

            if not rclpy.ok():
                rclpy.init()

            class FenNode(Node):
                def __init__(self):
                    super().__init__("fen_controller_bridge")

            self._node = FenNode()
            from geometry_msgs.msg import PoseStamped
            self._pub_l = self._node.create_publisher(PoseStamped, f"{ns}/left/pose",  10)
            self._pub_r = self._node.create_publisher(PoseStamped, f"{ns}/right/pose", 10)
            self._spin_thread = threading.Thread(
                target=lambda: rclpy.spin(self._node), daemon=True)
            self._spin_thread.start()
            logger.info("ROS2 node initialized")
        except Exception as e:
            logger.warning(f"ROS2 init failed: {e}, falling back to standalone")
            self.version = "none"

    def _init_ros1(self, ns):
        try:
            import rospy
            rospy.init_node("fen_controller_bridge", anonymous=False, disable_signals=True)
            from geometry_msgs.msg import PoseStamped
            self._pub_l = rospy.Publisher(f"{ns}/left/pose",  PoseStamped, queue_size=5)
            self._pub_r = rospy.Publisher(f"{ns}/right/pose", PoseStamped, queue_size=5)
            logger.info("ROS1 node initialized")
        except Exception as e:
            logger.warning(f"ROS1 init failed: {e}, falling back to standalone")
            self.version = "none"

    def publish(self, status: StatusPacket):
        if self.version == "none":
            return
        try:
            if self.version == "ros2":
                self._publish_ros2(status)
            elif self.version == "ros1":
                self._publish_ros1(status)
        except Exception as e:
            logger.debug(f"ROS publish error: {e}")

    def _publish_ros2(self, status: StatusPacket):
        from geometry_msgs.msg import PoseStamped
        import rclpy
        for side, pos, pub in [("left",  status.l_pos, self._pub_l),
                                ("right", status.r_pos, self._pub_r)]:
            msg = PoseStamped()
            msg.header.frame_id = f"{side}_base_link"
            msg.header.stamp = self._node.get_clock().now().to_msg()
            msg.pose.position.x = pos[0] / 1000.0  # mm → m
            msg.pose.position.y = pos[1] / 1000.0
            msg.pose.position.z = pos[2] / 1000.0
            pub.publish(msg)

    def _publish_ros1(self, status: StatusPacket):
        import rospy
        from geometry_msgs.msg import PoseStamped
        for side, pos, pub in [("left",  status.l_pos, self._pub_l),
                                ("right", status.r_pos, self._pub_r)]:
            msg = PoseStamped()
            msg.header.frame_id = f"{side}_base_link"
            msg.header.stamp = rospy.Time.now()
            msg.pose.position.x = pos[0] / 1000.0
            msg.pose.position.y = pos[1] / 1000.0
            msg.pose.position.z = pos[2] / 1000.0
            pub.publish(msg)

    def shutdown(self):
        if self.version == "ros2":
            try:
                import rclpy
                self._node.destroy_node()
                rclpy.shutdown()
            except Exception:
                pass
        elif self.version == "ros1":
            try:
                import rospy
                rospy.signal_shutdown("fen_bridge shutdown")
            except Exception:
                pass


# ─── 主桥接类 ───────────────────────────────────────────────────
class FenBridge:

    def __init__(self, cfg: dict, ros_version: str):
        self.cfg = cfg
        self._running = False
        self._esp32_addr: Optional[tuple] = None

        # UDP 套接字
        self._sock_recv = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock_recv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock_recv.bind((cfg["network"]["bind_ip"], UDP_PORT_CTRL))
        self._sock_recv.settimeout(1.0)

        self._sock_send = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # 确定 ROS 版本
        if ros_version == "auto":
            ros_version = _detect_ros_version()
        logger.info(f"ROS mode: {ros_version}")

        self._ros = RosPublisher(ros_version, cfg["ros"]["namespace"])

        # 解析手臂 IP
        left_ip, right_ip = self._resolve_arm_ips()

        # 创建双臂控制器
        self._arms = DualArmController(left_ip, right_ip, cfg)

        # 统计
        self._pkt_count  = 0
        self._last_pkt_t = time.time()

    def _resolve_arm_ips(self) -> tuple:
        """解析手臂 IP（自动发现 or 配置文件）"""
        disc_cfg  = self.cfg.get("discovery", {})
        left_def  = self.cfg["arms"]["left"]["ip"]
        right_def = self.cfg["arms"]["right"]["ip"]

        if disc_cfg.get("enabled", True):
            logger.info("Auto-discovering xArm controllers on LAN...")
            found = discover_xarms(
                subnet=disc_cfg.get("subnet", "192.168.1"),
                port=disc_cfg.get("xarm_port", 18333),
                timeout=disc_cfg.get("scan_timeout", 0.3),
                max_threads=disc_cfg.get("scan_threads", 30),
                progress_cb=lambda s, t: logger.debug(f"Scan {s}/{t}"),
            )
            if found:
                left_ip, right_ip = assign_arms(found, left_def, right_def)
                if left_ip and right_ip:
                    logger.info(f"Discovered → Left:{left_ip}  Right:{right_ip}")
                    return left_ip, right_ip
            logger.warning("Auto-discovery found nothing, using config IPs")

        logger.info(f"Using config IPs → Left:{left_def}  Right:{right_def}")
        return left_def, right_def

    def start(self):
        self._running = True
        logger.info("Connecting to both arms...")
        self._arms.connect_all()
        logger.info(f"Left  connected: {self._arms.left.connected}")
        logger.info(f"Right connected: {self._arms.right.connected}")

        # 状态反馈线程（10Hz）
        self._status_thread = threading.Thread(target=self._status_loop, daemon=True)
        self._status_thread.start()

        logger.info(f"Listening for gamepad on UDP:{UDP_PORT_CTRL} ...")
        self._recv_loop()

    def _recv_loop(self):
        """主接收循环（阻塞）"""
        while self._running:
            try:
                data, addr = self._sock_recv.recvfrom(64)
                self._esp32_addr = addr  # 记录 ESP32 地址，用于发状态包

                pkt = CtrlPacket.parse(data)
                if pkt is None:
                    continue

                self._pkt_count += 1
                if self._pkt_count % 200 == 0:
                    dt = time.time() - self._last_pkt_t
                    logger.debug(f"Recv rate: {200/dt:.1f} Hz  pkt={pkt}")
                    self._last_pkt_t = time.time()

                self._arms.process(pkt)

            except socket.timeout:
                continue
            except KeyboardInterrupt:
                break
            except Exception as e:
                logger.error(f"Recv error: {e}")

    def _status_loop(self):
        """定期向 ESP32 发送状态包"""
        while self._running:
            time.sleep(0.1)  # 10Hz
            if self._esp32_addr is None:
                continue
            try:
                status = self._arms.build_status()
                self._ros.publish(status)
                data = status.pack()
                esp_ip = self._esp32_addr[0]
                self._sock_send.sendto(data, (esp_ip, UDP_PORT_STATUS))
            except Exception as e:
                logger.debug(f"Status send error: {e}")

    def stop(self):
        self._running = False
        self._arms.disconnect_all()
        self._ros.shutdown()
        self._sock_recv.close()
        self._sock_send.close()
        logger.info("Bridge stopped.")


# ─── 入口 ───────────────────────────────────────────────────────
def load_config(path: str) -> dict:
    with open(path, "r") as f:
        return yaml.safe_load(f)


def main():
    parser = argparse.ArgumentParser(description="FeN Robot Controller Bridge")
    parser.add_argument("--config", default=os.path.join(
        os.path.dirname(__file__), "config", "config.yaml"))
    parser.add_argument("--ros-version", default="auto",
                        choices=["auto", "ros1", "ros2", "none"])
    parser.add_argument("--left-ip",  default=None, help="左臂 IP（覆盖配置文件）")
    parser.add_argument("--right-ip", default=None, help="右臂 IP（覆盖配置文件）")
    parser.add_argument("--no-discover", action="store_true", help="禁用自动发现")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    cfg = load_config(args.config)

    # 命令行参数覆盖
    if args.left_ip:  cfg["arms"]["left"]["ip"]  = args.left_ip
    if args.right_ip: cfg["arms"]["right"]["ip"] = args.right_ip
    if args.no_discover: cfg["discovery"]["enabled"] = False

    bridge = FenBridge(cfg, ros_version=args.ros_version)
    try:
        bridge.start()
    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    finally:
        bridge.stop()


if __name__ == "__main__":
    main()
