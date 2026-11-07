#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Refresh stopped, explicitly prepared QA apps while preserving their profiles.

Only app directories and setup metadata are replaced. Existing profile contents
are neither copied nor modified. The previous app copies remain for rollback.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile


SCRIPT_DIR = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("qa_launch", SCRIPT_DIR / "launch-acceptance-apps.py")
LAUNCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LAUNCH)


def require_stopped(setup_file, setup):
    for client in setup["clients"]:
        with socket.socket() as probe:
            probe.settimeout(0.5)
            if probe.connect_ex(("127.0.0.1", client["marionettePort"])) == 0:
                raise ValueError("Stop both prepared QA apps before refreshing")
    record = setup_file.parent / "processes.json"
    processes = json.loads(record.read_text()) if record.exists() else []
    for process in processes:
        pid = process.get("pid")
        if type(pid) is not int or pid <= 0:
            raise ValueError("Invalid prepared QA process record")
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            continue
        # A reaped browser can remain a zombie below a managed executor.
        stat = Path(f"/proc/{pid}/stat")
        if sys.platform == "linux" and stat.exists() and stat.read_text().rsplit(")", 1)[1].split()[0] == "Z":
            continue
        raise ValueError("A recorded QA process still exists; stop it before refreshing")


def replacement_record(original, prepared):
    result = {**original}
    for key in ("buildCommit", "buildTree", "buildWorkingTreeDirty", "engineSha256",
                "archiveSha256", "browserArchiveSha256", "resourceSha256"):
        result[key] = prepared[key]
    new_clients = {client["name"]: client for client in prepared["clients"]}
    result["clients"] = [
        {**client, "archiveSha256": new_clients[client["name"]]["archiveSha256"],
         "browserArchiveSha256": new_clients[client["name"]]["browserArchiveSha256"]}
        for client in original["clients"]
    ]
    return result


def replace_apps(setup_file, original, prepared, backup):
    updated = replacement_record(original, prepared)
    pairs = [(Path(original["app"]), Path(prepared["app"]))]
    replacements = {client["name"]: client for client in prepared["clients"]}
    pairs.extend((Path(client["app"]), Path(replacements[client["name"]]["app"]))
                 for client in original["clients"])
    completed = []
    try:
        for target, replacement in pairs:
            saved = backup / target.name
            target.rename(saved)
            try:
                replacement.rename(target)
            except Exception:
                saved.rename(target)
                raise
            completed.append((target, replacement, saved))
        temporary = setup_file.with_suffix(".refresh.json")
        temporary.write_text(json.dumps(updated, indent=2))
        temporary.replace(setup_file)
    except Exception:
        for target, replacement, saved in reversed(completed):
            target.rename(replacement)
            saved.rename(target)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    args = parser.parse_args()
    setup_file = args.setup.resolve(strict=True)
    original = json.loads(setup_file.read_text())
    LAUNCH.validate_setup(setup_file, original)
    base_name = "Floorp Workspace Sync QA.app" if original["platform"] == "macos" else "floorp-qa-base"
    if Path(original["app"]).resolve(strict=True) != setup_file.parent / base_name:
        parser.error("Base app must belong to the preparation directory")
    require_stopped(setup_file, original)
    os.umask(0o077)
    lock = setup_file.parent / ".apps-refreshing"
    lock.mkdir(mode=0o700)
    try:
        with tempfile.TemporaryDirectory(prefix="qa-app-refresh-", dir=setup_file.parent) as temporary:
            stage = Path(temporary) / "prepared"
            command = [sys.executable, str(SCRIPT_DIR / "prepare-acceptance-app.py"),
                       "--platform", original["platform"], "--source-app", original["sourceApp"],
                       "--build-root", str(args.build_root.resolve(strict=True)),
                       "--output-root", str(stage), "--proxy-url", ""]
            if original.get("sourceAppCommit"):
                command.extend(["--source-ref", original["sourceAppCommit"]])
            # Staging's disposable profiles are unused; retain trust/proxy settings
            # only in the original dedicated profiles, which this tool never writes.
            environment = {key: value for key, value in os.environ.items() if key != "NODE_EXTRA_CA_CERTS"}
            subprocess.run(command, env=environment, check=True, stdout=subprocess.DEVNULL)
            prepared = json.loads((stage / "setup.json").read_text())
            require_stopped(setup_file, original)
            backup = Path(tempfile.mkdtemp(prefix="previous-apps-", dir=setup_file.parent))
            (backup / "setup.json").write_text(json.dumps(original, indent=2))
            replace_apps(setup_file, original, prepared, backup)
    finally:
        lock.rmdir()
    LAUNCH.validate_setup(setup_file, json.loads(setup_file.read_text()))
    print(json.dumps({"refreshed": ["A", "B"], "setup": str(setup_file),
                      "previousApps": str(backup), "profilesPreserved": True}, indent=2))


if __name__ == "__main__":
    main()
