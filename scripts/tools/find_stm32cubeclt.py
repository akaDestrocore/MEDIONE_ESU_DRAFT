#!/usr/bin/env python3

import sys
import os
import glob
import json

def find_clt():
    if sys.platform == "win32":
        candidates = glob.glob("C:/ST/STM32CubeCLT_*")
    else:
        candidates = glob.glob("/opt/st/stm32cubeclt_*") + glob.glob("/opt/stm32cubeclt*")

    candidates = sorted(candidates, reverse=True)
    if not candidates:
        sys.exit("ERROR: No STM32CubeCLT installation found.")

    return candidates[0].replace("\\", "/")


def update_vscode_settings(clt_path, workspace_root):
    settings_path = os.path.join(workspace_root, ".vscode", "settings.json")
    os.makedirs(os.path.dirname(settings_path), exist_ok=True)

    settings = {}
    if os.path.exists(settings_path):
        with open(settings_path, "r") as f:
            try:
                settings = json.load(f)
            except json.JSONDecodeError:
                pass

    settings["stm32.cltPath"] = clt_path

    with open(settings_path, "w") as f:
        json.dump(settings, f, indent=4)

    print(f"STM32CubeCLT found: {clt_path}")
    print(f"Written to: {settings_path}")


if __name__ == "__main__":
    workspace_root = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
    clt = find_clt()
    update_vscode_settings(clt, workspace_root)