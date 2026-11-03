#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run headed CI arguments on an owned xvfb-run display with a private xfwm4."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

INTERRUPTED_SIGNAL = None


def process(pid):
    path = Path("/proc") / str(pid)
    try:
        stat = (path / "stat").read_text()
        fields = stat[stat.rindex(")") + 2:].split()
        return {"pid": int(pid), "ppid": int(fields[1]), "group": int(fields[2]),
                "session": int(fields[3]), "state": fields[0], "uid": path.stat().st_uid,
                "argv": [part.decode(errors="replace") for part in (path / "cmdline").read_bytes().split(b"\0") if part]}
    except (OSError, ValueError, IndexError):
        return None


def processes():
    return [value for path in Path("/proc").iterdir() if path.name.isdigit()
            if (value := process(path.name)) is not None]


def owned_display():
    display = os.environ.get("DISPLAY", "")
    match = re.fullmatch(r":(\d+)(?:\.0)?", display)
    if not match:
        raise RuntimeError("A local, fresh xvfb-run DISPLAY is required")
    server_display = ":" + match[1]
    ancestors = []
    pid = os.getppid()
    while pid > 1 and pid not in [item["pid"] for item in ancestors]:
        value = process(pid)
        if not value:
            break
        ancestors.append(value)
        pid = value["ppid"]
    owners = {item["pid"] for item in ancestors if item["uid"] == os.getuid()
              and any(Path(arg).name == "xvfb-run" for arg in item["argv"][:2])}
    servers = [item for item in processes() if item["uid"] == os.getuid()
               and item["ppid"] in owners and item["argv"]
               and Path(item["argv"][0]).name == "Xvfb"
               and server_display in item["argv"][1:]]
    if len(servers) != 1:
        raise RuntimeError("DISPLAY is not an Xvfb directly owned by this command's xvfb-run ancestor")
    return {"display": display, "xvfb": servers[0], "xvfbRun": process(servers[0]["ppid"])}


def xprop(*arguments):
    result = subprocess.run(["xprop", *arguments], capture_output=True, text=True, timeout=5)
    if result.returncode:
        raise RuntimeError("xprop could not inspect the owned display: " + result.stderr.strip())
    return result.stdout.strip()


def manager_window():
    output = xprop("-root", "_NET_SUPPORTING_WM_CHECK")
    match = re.search(r"window id # (0x[0-9a-fA-F]+)", output)
    return match[1] if match and int(match[1], 16) else None


def group_members(group):
    return [item for item in processes() if item["group"] == group and item["state"] != "Z"]


def stop_owned_group(child, grace=4):
    """Each supplied process was created here with start_new_session=True."""
    if child is None:
        return {"remaining": []}
    for sig, bound in [(signal.SIGTERM, grace), (signal.SIGKILL, 2)]:
        members = group_members(child.pid)
        if not members:
            break
        if any(item["uid"] != os.getuid() or item["session"] != child.pid for item in members):
            raise RuntimeError("Refusing to signal a process group outside the owned session")
        try:
            os.killpg(child.pid, sig)
        except ProcessLookupError:
            break
        deadline = time.monotonic() + bound
        while time.monotonic() < deadline and group_members(child.pid):
            time.sleep(0.05)
    try:
        child.wait(timeout=1)
    except subprocess.TimeoutExpired:
        pass
    return {"group": child.pid, "exitCode": child.poll(), "remaining": group_members(child.pid)}


def cleanup_group(child, grace=4):
    try:
        return stop_owned_group(child, grace)
    except Exception as error:
        return {"error": str(error), "remaining": group_members(child.pid) if child else []}


def begin_cleanup():
    # A second job-cancellation signal must not interrupt bounded cleanup.
    for sig in [signal.SIGTERM, signal.SIGINT]:
        signal.signal(sig, signal.SIG_IGN)


def write_report(path, value):
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def interrupted(signum, _frame):
    # Do not throw between Popen creating a group and returning its handle.
    # The supervisor checks this flag before/after bounded startup operations.
    global INTERRUPTED_SIGNAL
    INTERRUPTED_SIGNAL = signum


def check_interrupted():
    if INTERRUPTED_SIGNAL is not None:
        raise InterruptedError(f"Interrupted by signal {INTERRUPTED_SIGNAL}")


def run_private_session(output, command):
    report = {"passed": False, "command": command, "privateSessionBus": bool(os.environ.get("DBUS_SESSION_BUS_ADDRESS"))}
    manager = child = None
    try:
        report["displayOwnership"] = owned_display()
        if not report["privateSessionBus"]:
            raise RuntimeError("The supervisor must run inside its private dbus-run-session")
        if manager_window() is not None:
            raise RuntimeError("Refusing a display which already has a window manager")
        manager_command = [shutil.which("xfwm4"), "--sm-client-disable", "--compositor=off"]
        report["windowManagerCommand"] = manager_command
        with (output / "window-manager.log").open("w") as log:
            check_interrupted()
            manager = subprocess.Popen(manager_command, stdout=log, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            report["windowManagerPid"] = manager.pid
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                check_interrupted()
                if manager.poll() is not None:
                    raise RuntimeError(f"Owned xfwm4 exited before startup: {manager.returncode}")
                window = manager_window()
                if window:
                    self_check = xprop("-id", window, "_NET_SUPPORTING_WM_CHECK")
                    name = xprop("-id", window, "_NET_WM_NAME")
                    pid = xprop("-id", window, "_NET_WM_PID")
                    reported_pid = re.search(r"=\s*(\d+)", pid)
                    if not re.search(r"window id # " + re.escape(window) + r"\b", self_check) or "xfwm4" not in name.lower():
                        raise RuntimeError("The display's WM identity does not match the owned xfwm4")
                    if reported_pid and int(reported_pid[1]) != manager.pid:
                        raise RuntimeError("The display's WM PID does not match the owned xfwm4")
                    report["windowManagerIdentity"] = {"window": window, "selfCheck": self_check, "name": name,
                                                       "pidProperty": pid, "process": process(manager.pid)}
                    break
                time.sleep(0.05)
            else:
                raise RuntimeError("Owned xfwm4 did not publish _NET_SUPPORTING_WM_CHECK within 15 seconds")
            child = subprocess.Popen(command, start_new_session=True)
            report["childPid"] = child.pid
            while child.poll() is None:
                check_interrupted()
                if manager.poll() is not None:
                    raise RuntimeError("Owned xfwm4 exited while the headed child was running")
                time.sleep(0.05)
            report["childExitCode"] = child.returncode
            report["passed"] = child.returncode == 0
    except Exception as error:
        report["error"] = str(error)
    finally:
        begin_cleanup()
        report["childCleanup"] = cleanup_group(child)
        report["windowManagerCleanup"] = cleanup_group(manager)
        if any(value.get("error") or value["remaining"] for value in
               [report["childCleanup"], report["windowManagerCleanup"]]):
            report["passed"] = False
        write_report(output / "session.json", report)
    code = report.get("childExitCode", 1)
    return (code if code >= 0 else 128 - code) if report["passed"] or code else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--_private-session", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("This wrapper is only for Linux headed CI")
    if len(args.command) < 2 or args.command[0] != "--":
        parser.error("Pass the child argv after --")
    command = args.command[1:]
    for name in ["xfwm4", "dbus-run-session", "xprop"]:
        if not shutil.which(name):
            parser.error("Missing required executable: " + name)
    for sig in [signal.SIGTERM, signal.SIGINT]:
        signal.signal(sig, interrupted)
    if args._private_session:
        return run_private_session(args.output, command)
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="window-manager-", dir=args.output)).resolve()
    report = {"passed": False, "command": command, "output": str(output), "scope": "owned Linux headed test WM only"}
    session = None
    try:
        report["displayOwnership"] = owned_display()
        if manager_window() is not None:
            raise RuntimeError("Refusing a display which already has a window manager")
        env = {**os.environ}
        for key, name in [("XDG_CONFIG_HOME", "config"), ("XDG_CACHE_HOME", "cache"),
                          ("XDG_DATA_HOME", "data"), ("XDG_RUNTIME_DIR", "runtime")]:
            directory = output / name
            directory.mkdir(mode=0o700)
            env[key] = str(directory)
        env.pop("SESSION_MANAGER", None)
        env.pop("DBUS_SESSION_BUS_ADDRESS", None)
        bus_command = [shutil.which("dbus-run-session"), "--", sys.executable, str(Path(__file__).resolve()),
                       "--_private-session", "--output", str(output), "--", *command]
        report["privateSessionCommand"] = bus_command
        with (output / "dbus.log").open("w") as log:
            check_interrupted()
            session = subprocess.Popen(bus_command, env=env, stderr=log, start_new_session=True)
            report["privateSessionPid"] = session.pid
            while session.poll() is None:
                check_interrupted()
                time.sleep(0.05)
            session.wait()
        report["sessionExitCode"] = session.returncode
        session_report = output / "session.json"
        report["session"] = json.loads(session_report.read_text()) if session_report.is_file() else None
        report["passed"] = session.returncode == 0 and report["session"] is not None and report["session"].get("passed") is True
    except Exception as error:
        report["error"] = str(error)
    finally:
        begin_cleanup()
        # The inner supervisor may need both of its bounded child/WM cleanups
        # after a job cancellation, before its private bus group can be killed.
        report["privateSessionCleanup"] = cleanup_group(session, grace=15)
        if report["privateSessionCleanup"].get("error") or report["privateSessionCleanup"]["remaining"]:
            report["passed"] = False
        write_report(output / "report.json", report)
        print("WINDOW_MANAGER_REPORT=" + str(output / "report.json"), flush=True)
    code = report.get("sessionExitCode", 1)
    return (code if code >= 0 else 128 - code) if report["passed"] or code else 1


if __name__ == "__main__":
    raise SystemExit(main())
