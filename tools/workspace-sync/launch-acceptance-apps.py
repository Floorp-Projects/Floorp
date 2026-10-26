#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Launch the explicitly prepared QA apps with their exact dedicated profiles."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import socket
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", type=Path, required=True)
    parser.add_argument("--client", choices=["A", "B"])
    args = parser.parse_args()
    setup_file = args.setup.resolve(strict=True)
    setup = json.loads(setup_file.read_text())
    selected = [item for item in setup["clients"] if not args.client or item["name"] == args.client]
    for item in selected:
        name = item["name"]
        if name not in ["A", "B"]:
            raise ValueError("Unknown dedicated client")
        profile = Path(item["profile"]).resolve(strict=True)
        app = Path(item["app"]).resolve(strict=True)
        binary = Path(item["binary"]).resolve(strict=True)
        if profile != setup_file.parent / f"profile-{name.lower()}" or app.parent != setup_file.parent:
            raise ValueError("QA resources must belong to the preparation directory")
        if binary != app / "Contents/MacOS/floorp":
            raise ValueError("Expected the prepared complete Floorp app")
        info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
        if info["CFBundleIdentifier"] != f"org.floorp.workspace-sync.qa.{name.lower()}":
            raise ValueError("Expected the separate QA app identity")
        prefs = (profile / "user.js").read_text()
        if ('user_pref("remote.prefs.recommended", false)' not in prefs or
                f'user_pref("floorp.workspaces.sync.acceptance-profile", "{name}")' not in prefs):
            raise ValueError("Live QA startup preferences are missing")
        with socket.socket() as probe:
            probe.settimeout(0.5)
            if probe.connect_ex(("127.0.0.1", item["marionettePort"])) == 0:
                raise RuntimeError(f"QA client {name}'s port is already in use")
    processes = []
    for item in selected:
        process = subprocess.Popen([
            item["binary"], "--no-remote", "--remote-allow-system-access", "--marionette",
            "--profile", item["profile"], "about:preferences#sync",
        ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True,
            env={**os.environ, "MOZ_REMOTE_ALLOW_SYSTEM_ACCESS": "1"})
        processes.append({"name": item["name"], "pid": process.pid,
                          "profile": item["profile"], "port": item["marionettePort"], "app": item["app"]})
    (setup_file.parent / "processes.json").write_text(json.dumps(processes, indent=2))
    print(json.dumps({"launched": [{"name": item["name"], "pid": item["pid"]} for item in processes]}, indent=2))


if __name__ == "__main__":
    main()
