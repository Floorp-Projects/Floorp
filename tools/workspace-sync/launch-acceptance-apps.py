#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Launch only the explicitly prepared QA apps and their dedicated profiles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import socket
import subprocess

from acceptance_app_support import app_layout


def validate_setup(setup_file, setup, selected_name=None):
    if (setup_file.parent / ".apps-refreshing").exists():
        raise ValueError("Prepared QA apps are being refreshed")
    platform = setup.get("platform", "macos")
    clients = setup.get("clients", [])
    if len(clients) != 2 or {item.get("name") for item in clients} != {"A", "B"}:
        raise ValueError("Exactly two named QA clients are required")
    ports = [item["marionettePort"] for item in clients]
    if len(set(ports)) != 2 or any(type(port) is not int or not 0 < port < 65536 for port in ports):
        raise ValueError("Distinct valid QA ports are required")
    selected = [item for item in clients if not selected_name or item["name"] == selected_name]
    for item in selected:
        name = item["name"]
        profile = Path(item["profile"]).resolve(strict=True)
        app = Path(item["app"]).resolve(strict=True)
        binary = Path(item["binary"]).resolve(strict=True)
        expected_name = f"Floorp Sync QA {name}.app" if platform == "macos" else f"floorp-qa-{name.lower()}"
        if profile != setup_file.parent / f"profile-{name.lower()}" or app != setup_file.parent / expected_name:
            raise ValueError("QA resources must belong to the preparation directory")
        resources, expected_binary = app_layout(app, platform)
        if binary != expected_binary or not os.access(binary, os.X_OK):
            raise ValueError("Expected the prepared complete Floorp app")
        identity = f"org.floorp.workspace-sync.qa.{name.lower()}"
        if platform == "macos":
            info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
            if info["CFBundleIdentifier"] != identity:
                raise ValueError("Expected the separate QA app identity")
        else:
            marker = json.loads((app / "workspace-sync-qa.json").read_text())
            if marker != {"schemaVersion": 1, "client": name, "identity": identity}:
                raise ValueError("Expected the separate QA app identity")
        if item.get("archiveSha256") and hashlib.sha256((resources / "omni.ja").read_bytes()).hexdigest() != item["archiveSha256"]:
            raise ValueError("Prepared QA archive changed after verification")
        if item.get("browserArchiveSha256") and hashlib.sha256((resources / "browser/omni.ja").read_bytes()).hexdigest() != item["browserArchiveSha256"]:
            raise ValueError("Prepared QA browser archive changed after verification")
        prefs = (profile / "user.js").read_text()
        for required in (
            'user_pref("remote.prefs.recommended", false);',
            f'user_pref("floorp.workspaces.sync.acceptance-profile", "{name}");',
            f'user_pref("marionette.port", {item["marionettePort"]});',
        ):
            if required not in prefs:
                raise ValueError("Live QA startup preferences are missing")
        if setup.get("proxyConfigured") and 'user_pref("network.proxy.type", 1);' not in prefs:
            raise ValueError("Prepared QA proxy preferences are missing")
        if item.get("caCertificateSha256") and not (profile / "cert9.db").is_file():
            raise ValueError("Prepared QA certificate database is missing")
    return selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", type=Path, required=True)
    parser.add_argument("--client", choices=["A", "B"])
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--wait", action="store_true",
                        help="Keep the launcher alive while its apps run (for managed execution sessions)")
    parser.add_argument("--open-settings", action="store_true",
                        help="Open account settings even when restarting an existing QA session")
    args = parser.parse_args()
    setup_file = args.setup.resolve(strict=True)
    setup = json.loads(setup_file.read_text())
    selected = validate_setup(setup_file, setup, args.client)
    for item in selected:
        with socket.socket() as probe:
            probe.settimeout(0.5)
            if probe.connect_ex(("127.0.0.1", item["marionettePort"])) == 0:
                raise RuntimeError(f"QA client {item['name']}'s port is already in use")
    processes = []
    owned = []
    try:
        for item in selected:
            command = [item["binary"], "--no-remote", "--purgecaches", "--remote-allow-system-access", "--marionette",
                       "--profile", item["profile"]]
            if args.headless:
                command.append("--headless")
            if args.open_settings or not (Path(item["profile"]) / "prefs.js").exists():
                command.append("about:preferences#sync")
            process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                       start_new_session=True,
                                       env={**os.environ, "MOZ_REMOTE_ALLOW_SYSTEM_ACCESS": "1"})
            owned.append(process)
            processes.append({"name": item["name"], "pid": process.pid,
                              "profile": item["profile"], "port": item["marionettePort"], "app": item["app"]})
    except Exception:
        for process in owned:
            process.terminate()
        raise
    record = setup_file.parent / "processes.json"
    previous = json.loads(record.read_text()) if record.exists() else []
    selected_names = {item["name"] for item in selected}
    record.write_text(json.dumps([item for item in previous if item["name"] not in selected_names] + processes, indent=2))
    print(json.dumps({"launched": [{"name": item["name"], "pid": item["pid"]} for item in processes]}, indent=2), flush=True)
    if args.wait:
        try:
            for process in owned:
                process.wait()
        except KeyboardInterrupt:
            for process in owned:
                if process.poll() is None:
                    process.terminate()
            raise


if __name__ == "__main__":
    main()
