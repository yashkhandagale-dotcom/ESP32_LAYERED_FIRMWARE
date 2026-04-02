"""
find_esp32.py
-------------
Works two ways:
  1. As PlatformIO pre-script  → auto-runs before every OTA upload
  2. As standalone script      → python find_esp32.py

ESP32 broadcasts "I_AM_ESP32:<ip>" on UDP port 8889 at every boot.
This script catches that, updates platformio.ini, no mDNS needed.

Place this file in your project root (same folder as platformio.ini)
"""

import socket
import re
import os
import sys

UDP_PORT  = 8889
INI_FILE  = "platformio.ini"
TIMEOUT_S = 30


def find_esp32_ip():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(TIMEOUT_S)
    sock.bind(("", UDP_PORT))

    print(f"\n[Discovery] Listening on UDP:{UDP_PORT} for ESP32...")
    print(f"[Discovery] Waiting {TIMEOUT_S}s — reset ESP32 if needed\n")

    try:
        data, addr = sock.recvfrom(128)
        msg = data.decode().strip()
        print(f"[Discovery] Received: '{msg}'")

        if msg.startswith("I_AM_ESP32:"):
            ip = msg.split(":")[1].strip()
            print(f"[Discovery] ESP32 found at: {ip}\n")
            return ip

        print("[Discovery] Unknown message, ignoring")
        return None

    except socket.timeout:
        print("[Discovery] Timeout — ESP32 not found")
        return None

    finally:
        sock.close()


def update_ini(ip):
    if not os.path.exists(INI_FILE):
        print(f"[Discovery] {INI_FILE} not found")
        return False

    with open(INI_FILE, "r") as f:
        content = f.read()

    updated = re.sub(r"(upload_port\s*=\s*)[\w\.\-]+", f"\\g<1>{ip}", content)

    if updated == content:
        print(f"[Discovery] upload_port unchanged")
        return False

    with open(INI_FILE, "w") as f:
        f.write(updated)

    print(f"[Discovery] platformio.ini → upload_port = {ip}")
    return True


# ── PlatformIO pre-script entry point ──────────────────────────────
# When used as extra_scripts = pre:find_esp32.py, PlatformIO calls
# this file in a SCons environment. We hook into the upload target.
try:
    Import("env")  # SCons import — only works inside PlatformIO

    def before_upload(source, target, env):
        ip = find_esp32_ip()
        if ip:
            update_ini(ip)
            env.Replace(UPLOAD_PORT=ip)
            print(f"[Discovery] Upload port set to: {ip}")
        else:
            print("[Discovery] Could not find ESP32, using existing upload_port")

    env.AddPreAction("upload", before_upload)

except:
    # ── Standalone script entry point ──────────────────────────────
    # Running as: python find_esp32.py
    if __name__ == "__main__":
        ip = find_esp32_ip()
        if ip is None:
            sys.exit(1)
        update_ini(ip)
        print("[Discovery] Done! Run OTA upload now.")