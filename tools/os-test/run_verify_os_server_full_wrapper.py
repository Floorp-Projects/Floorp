#!/usr/bin/env python3
"""
Start Floorp Debug (`deno task feles-build dev`), wait for the OS server health
endpoint to come up, run `verify_os_server_full.py`, and tear everything down.
"""

from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[2]
DEFAULT_CMD = ["deno", "task", "feles-build", "dev"]


def wait_for_health(timeout: float, interval: float) -> bool:
    from verify_os_server_full import make_request  # type: ignore

    deadline = time.time() + timeout
    while time.time() < deadline:
        status, _ = make_request("/health")
        if status == 200:
            return True
        time.sleep(interval)
    return False


def stop_process(proc: subprocess.Popen[str], grace: float = 10.0) -> None:
    if proc.poll() is not None:
        return
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except Exception:
        try:
            proc.terminate()
        except Exception:
            pass
    try:
        proc.wait(timeout=grace)
        return
    except subprocess.TimeoutExpired:
        pass
    try:
        os.killpg(proc.pid, signal.SIGKILL)
    except Exception:
        try:
            proc.kill()
        except Exception:
            pass
    try:
        proc.wait(timeout=3)
    except Exception:
        pass


def run_verifier() -> int:
    import verify_os_server_full as verifier

    try:
        verifier.main()
        return 0
    except SystemExit as exc:  # noqa: BLE001
        code = exc.code
        if code is None:
            return 0
        return int(code)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run Floorp OS server verification end-to-end")
    parser.add_argument(
        "--cmd",
        nargs=argparse.REMAINDER,
        help="Custom command to start Floorp Debug (default: deno task feles-build dev)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=120.0,
        help="Seconds to wait for /health to respond",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=2.0,
        help="Seconds between health checks",
    )
    parser.add_argument(
        "--skip-start",
        action="store_true",
        help="Do not start Floorp Debug; assume it is already running",
    )

    args = parser.parse_args(argv)

    cmd = args.cmd if args.cmd else DEFAULT_CMD
    proc: subprocess.Popen[str] | None = None

    if args.skip_start:
        print("Skipping Floorp Debug start; assuming server is already running.")
    else:
        print(f"Starting Floorp Debug: {' '.join(cmd)} (cwd={ROOT_DIR})")
        proc = subprocess.Popen(
            cmd,
            cwd=ROOT_DIR,
            stdin=subprocess.DEVNULL,
            stdout=None,
            stderr=None,
            text=True,
            start_new_session=True,
        )

        print("Waiting for OS server health endpoint...")
        if not wait_for_health(args.timeout, args.interval):
            print("OS server did not become healthy within timeout.", file=sys.stderr)
            stop_process(proc)
            return 1
        print("OS server is healthy; waiting 20s for full startup...")
        time.sleep(20)
        print("Proceeding with verification.")

    exit_code = run_verifier()

    if proc is not None:
        print("Stopping Floorp Debug...")
        stop_process(proc)

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
