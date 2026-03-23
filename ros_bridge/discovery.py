"""
局域网自动发现 xArm 控制器
扫描指定网段，检测 xArm 控制器端口（默认 18333）
"""
import socket
import threading
import logging
from typing import List, Optional

logger = logging.getLogger("fen.discovery")

XARM_PORT = 18333
XARM_PING_TIMEOUT = 0.3


def _probe_ip(ip: str, port: int, timeout: float, results: list, lock: threading.Lock):
    """尝试 TCP 连接到指定 IP:port，成功则记录"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(timeout)
        if s.connect_ex((ip, port)) == 0:
            with lock:
                results.append(ip)
            logger.info(f"  Found xArm at {ip}:{port}")
        s.close()
    except Exception:
        pass


def discover_xarms(
    subnet: str = "192.168.1",
    port: int = XARM_PORT,
    timeout: float = XARM_PING_TIMEOUT,
    max_threads: int = 30,
    progress_cb=None,
) -> List[str]:
    """
    扫描 subnet.1 ~ subnet.254，返回响应 xArm 端口的 IP 列表（已排序）。

    Args:
        subnet:      如 "192.168.1"
        port:        xArm 控制器端口（默认 18333）
        timeout:     每个 IP 的连接超时（秒）
        max_threads: 并发线程数
        progress_cb: 可选回调 progress_cb(scanned, total)

    Returns:
        已排序的 IP 列表
    """
    results: List[str] = []
    lock = threading.Lock()
    ips = [f"{subnet}.{i}" for i in range(1, 255)]
    total = len(ips)
    scanned = 0

    def worker(batch):
        nonlocal scanned
        threads = []
        for ip in batch:
            t = threading.Thread(target=_probe_ip, args=(ip, port, timeout, results, lock))
            t.daemon = True
            t.start()
            threads.append(t)
        for t in threads:
            t.join()
        with lock:
            scanned += len(batch)
        if progress_cb:
            progress_cb(scanned, total)

    # 分批扫描
    batch_size = max_threads
    for i in range(0, total, batch_size):
        worker(ips[i:i + batch_size])

    results.sort(key=lambda ip: int(ip.split(".")[-1]))
    logger.info(f"Discovery done: found {len(results)} xArm(s): {results}")
    return results


def assign_arms(found: List[str], left_default: str, right_default: str):
    """
    从发现列表中分配左右臂 IP。
    优先使用默认 IP（如果在发现列表中），否则按顺序分配。

    Returns:
        (left_ip, right_ip) 或 (None, None) 如果未找到
    """
    if not found:
        return None, None

    left = left_default  if left_default  in found else None
    right = right_default if right_default in found else None

    remaining = [ip for ip in found if ip not in (left, right)]

    if left is None and remaining:
        left = remaining.pop(0)
    if right is None and remaining:
        right = remaining.pop(0)

    return left, right


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    print("Scanning for xArm controllers...")
    found = discover_xarms(progress_cb=lambda s, t: print(f"\r{s}/{t}", end="", flush=True))
    print(f"\nFound: {found}")
