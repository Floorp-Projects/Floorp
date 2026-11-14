#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0

"""Start Floorp Debug, wait for the OS-Server, run the full verifier, tear down.

Python parity with tools/os-test/run_verify_os_server_full_wrapper.ts:
- explicit --base-url / FLOORP_OS_BASE_URL precedence;
- explicit --token-file / FLOORP_OS_TOKEN_FILE precedence;
- literal-loopback target validation before any browser start when a token file
  is configured, while the token file itself may be published only after the
  browser starts;
- a same-origin, proxy-bypassing, redirect-refusing reachability check that
  accepts 200 or 401;
- one reconciled base URL / token file passed to the verifier, never duplicate
  conflicting flags from the verifier scope.
"""

from __future__ import annotations

import os
import signal
import subprocess
import sys
import time
from pathlib import Path

from os_test_client import (
    DEFAULT_BASE_URL,
    OsTestClientError,
    OsTestConfigError,
    RedactedArgumentParser,
    create_os_test_client,
    reconcile_os_test_client_args,
    resolve_base_url,
    resolve_token_file,
    validate_authenticated_base_url,
)

ROOT_DIR = Path(__file__).resolve().parents[2]
DEFAULT_CMD = ["deno", "task", "feles-build", "dev"]


def split_verifier_args(args: list[str]) -> tuple[list[str], list[str]]:
    """Split wrapper arguments from verifier arguments at the first bare --."""
    try:
        separator = args.index("--")
    except ValueError:
        return list(args), []
    return list(args[:separator]), list(args[separator + 1:])


def wait_for_health(
    base_url: str,
    timeout: float,
    interval: float,
    token_file: str | None = None,
) -> bool:
    """Wait for a same-origin OS-Server reachability signal.

    A bearer-protected /health may answer 401 before the explicit token file is
    published; that still proves a live server. When the token file exists, the
    shared client uses it with same-origin checks, ambient-proxy bypass and
    manual redirect refusal. 200 or 401 is accepted; every other status or a
    transport/redirect/origin failure retries until the deadline.
    """
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            token_path = (
                token_file
                if token_file is not None and os.path.exists(token_file)
                else None
            )
            client = create_os_test_client(base_url, token_path)
            status, _body = client.request(
                "/health",
                timeout=max(2.0, interval),
            )
            if status == 200:
                return True
            if status == 401:
                print(
                    "OS server reachable (401 bearer-protected /health); "
                    "authenticated checks run in verify_os_server_full.py."
                )
                return True
        except OsTestClientError:
            pass
        time.sleep(interval)
    return False


def stop_process(proc: subprocess.Popen[str], grace: float = 10.0) -> None:
    if proc.poll() is not None:
        return

    if os.name == "nt":
        try:
            subprocess.run(
                ["taskkill", "/PID", str(proc.pid), "/T", "/F"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            )
        except Exception:
            pass
        try:
            proc.wait(timeout=grace)
            return
        except subprocess.TimeoutExpired:
            pass
        try:
            proc.kill()
        except Exception:
            pass
        try:
            proc.wait(timeout=3)
        except Exception:
            pass
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


def run_verifier(verifier_args: list[str] | None = None) -> int:
    import verify_os_server_full as verifier

    try:
        return int(verifier.main(verifier_args))
    except SystemExit as exc:  # noqa: BLE001
        code = exc.code
        if code is None:
            return 0
        return int(code)


def main(argv: list[str] | None = None) -> int:
    args_list = list(sys.argv[1:] if argv is None else argv)
    try:
        wrapper_part, verifier_part = split_verifier_args(args_list)
        target_args = reconcile_os_test_client_args(wrapper_part, verifier_part)
    except OsTestConfigError as error:
        print(str(error), file=sys.stderr)
        return 2

    parser = RedactedArgumentParser(
        description="Run Floorp OS server verification end-to-end"
    )
    parser.add_argument(
        "--base-url",
        default=None,
        help="OS server base URL (default: FLOORP_OS_BASE_URL or "
        + DEFAULT_BASE_URL
        + ")",
    )
    parser.add_argument(
        "--token-file",
        default=None,
        help="Explicit OS-Server bearer token file (default: FLOORP_OS_TOKEN_FILE)",
    )
    parser.add_argument(
        "--cmd",
        nargs="+",
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
    try:
        args = parser.parse_args(target_args.wrapper_args)
    except OsTestConfigError as error:
        print(str(error), file=sys.stderr)
        return 2

    # Ambient env is read only after --help has exited, never at module import.
    base_url = resolve_base_url(
        target_args.base_url_flag,
        os.environ.get("FLOORP_OS_BASE_URL"),
    )
    try:
        token_file = resolve_token_file(
            target_args.token_file_flag,
            os.environ.get("FLOORP_OS_TOKEN_FILE"),
        )
        # Reject an unsafe authenticated target before starting any browser. The
        # token file itself may still be published only after browser startup.
        if token_file is not None:
            validate_authenticated_base_url(base_url)
    except OsTestConfigError as error:
        print(str(error), file=sys.stderr)
        return 2

    timeout = max(1.0, args.timeout)
    interval = max(1.0, args.interval)
    cmd = args.cmd if args.cmd else list(DEFAULT_CMD)
    proc: subprocess.Popen[str] | None = None

    if args.skip_start:
        print(
            "Skipping Floorp Debug start; assuming server is already running at "
            f"{base_url}."
        )
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
        if not wait_for_health(base_url, timeout, interval, token_file):
            print("OS server did not become healthy within timeout.", file=sys.stderr)
            stop_process(proc)
            return 1
        print("OS server is healthy; waiting 20s for full startup...")
        time.sleep(20)
        print("Proceeding with verification.")

    verifier_args = ["--base-url", base_url]
    if token_file is not None:
        verifier_args.extend(["--token-file", token_file])
    verifier_args.extend(target_args.verifier_args)

    exit_code = 1
    try:
        if verifier_args:
            print(f"Passing verifier args: {' '.join(verifier_args)}")
        exit_code = run_verifier(verifier_args)
    finally:
        if proc is not None:
            print("Stopping Floorp Debug...")
            stop_process(proc)

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
