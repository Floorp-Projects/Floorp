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
- TS --port-offset parity: derived dev OS-Server target 5175 + offset, the
  packaged production default 58261 for --skip-start without an offset, and
  offset validation/probing before token IO or browser spawn;
- one reconciled base URL / token file passed to the verifier, never duplicate
  conflicting flags from the verifier scope.
"""

from __future__ import annotations

import json
import os
import re
import signal
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from os_test_client import (
    DEFAULT_BASE_URL,
    OsTestClientError,
    OsTestConfigError,
    RedactedArgumentParser,
    create_os_test_client,
    reconcile_os_test_client_args,
    resolve_token_file,
    validate_authenticated_base_url,
)

ROOT_DIR = Path(__file__).resolve().parents[2]
DEFAULT_CMD = ["deno", "task", "feles-build", "dev"]

# Mirrors libs/shared/dev-ports/defines.ts and tools/src/port_offset.ts.
# Numeric values are copied, not re-invented: DEV_PORTS names/values,
# min/max/span, MAX_HTTP_PORT, DEV_PORT_OFFSET_MAX, DEV_OS_SERVER_PORT,
# OS_SERVER_PRODUCTION_DEFAULT_PORT.
DEV_PORTS = (5173, 5174, 5179, 5180, 5181, 5183, 5185, 5186, 5187, 5188, 5192)
DEV_PORT_MIN = min(DEV_PORTS)
DEV_PORT_MAX = max(DEV_PORTS)
DEV_PORT_NON_OVERLAP_SPAN = DEV_PORT_MAX - DEV_PORT_MIN + 1
MAX_HTTP_PORT = 65535
DEV_PORT_OFFSET_MAX = MAX_HTTP_PORT - DEV_PORT_MAX
DEV_OS_SERVER_PORT = 5175
OS_SERVER_PRODUCTION_DEFAULT_PORT = 58261


@dataclass
class OsVerifyOptions:
    base_url: str
    start_command_args: list[str]
    port_offset: int


def dev_port_offset_error(value: object) -> str | None:
    """Mirror launchPortOffsetError's user-facing validation."""
    if value is None:
        return "no port offset given"
    text = str(value).strip()
    if not re.fullmatch(r"[0-9]+", text):
        return 'port offset must be a non-negative ASCII integer or "auto"'
    normalized = text.lstrip("0") or "0"
    if len(normalized) > len(str(DEV_PORT_OFFSET_MAX)):
        return "port offset exceeds the valid HTTP port range"
    offset = int(normalized)
    if offset < 0:
        return f"port offset must be >= 0, got {json.dumps(value)}"
    if offset > DEV_PORT_OFFSET_MAX:
        return (
            f"port offset too large: {DEV_PORT_MAX} + {offset} "
            f"exceeds port {MAX_HTTP_PORT}"
        )
    if 0 < offset < DEV_PORT_NON_OVERLAP_SPAN:
        return (
            f"port offset {offset} is below the non-overlap span "
            f"{DEV_PORT_NON_OVERLAP_SPAN}; concurrent dev/test launch port sets "
            f"would overlap (use 0 or >= {DEV_PORT_NON_OVERLAP_SPAN})"
        )
    if uses_packaged_os_server_port(offset):
        return (
            f"--port-offset {offset} maps a dev port to the installed Floorp "
            f"OS-Server port {OS_SERVER_PRODUCTION_DEFAULT_PORT}; "
            "use a different offset or auto"
        )
    return None


def os_server_port_for_offset(offset: int) -> int:
    """Mirror osServerPortForOffset; shared dev-port validation applies."""
    error = dev_port_offset_error(offset)
    if error is not None:
        raise OsTestConfigError(error)
    return DEV_OS_SERVER_PORT + offset


def dev_os_server_base_url(offset: int) -> str:
    return f"http://127.0.0.1:{os_server_port_for_offset(offset)}"


def parse_port_offset_spec(raw: str | int | None) -> int | str:
    """Mirror parsePortOffsetSpec: absent/empty->0, auto, validated numeric."""
    if raw is None or raw == "":
        return 0
    if raw == "auto":
        return "auto"
    if isinstance(raw, int) or re.fullmatch(r"[0-9]+", str(raw).strip()):
        error = dev_port_offset_error(raw)
        if error is not None:
            raise OsTestConfigError(error)
        return int(str(raw).strip().lstrip("0") or "0")
    raise OsTestConfigError("--port-offset must be an ASCII integer or auto")


def parse_wrapper_port_offset_arg(
    raw: str | None,
    skip_start: bool,
) -> int | str | None:
    """Mirror parseWrapperPortOffsetArg, including the skip-start/auto edge."""
    if raw is None:
        return None
    if not str(raw).strip():
        raise OsTestConfigError("--port-offset requires a value <n|auto>")
    spec = parse_port_offset_spec(raw)
    if spec == "auto" and skip_start:
        raise OsTestConfigError(
            "--port-offset auto cannot be combined with --skip-start: auto "
            "resolves a free launch topology, not the offset of an "
            "already-running instance (pass the concrete offset)"
        )
    return spec


def resolve_os_verify_options(
    base_url_flag: str | None,
    env_base_url: str | None,
    port_offset: int | None,
    skip_start: bool,
) -> OsVerifyOptions:
    """Mirror resolveOsVerifyOptions; explicit base URL always wins."""
    if port_offset is not None:
        os_server_port_for_offset(port_offset)

    flag = base_url_flag.strip() if base_url_flag else None
    env = (env_base_url.strip() or None) if env_base_url else None
    explicit = flag or env

    targets_dev_launch = not skip_start or port_offset is not None
    if targets_dev_launch:
        base_url = (
            explicit
            if explicit is not None
            else dev_os_server_base_url(port_offset if port_offset is not None else 0)
        )
        start_args = (
            []
            if skip_start or port_offset is None
            else ["--port-offset", str(port_offset)]
        )
        return OsVerifyOptions(
            base_url,
            start_args,
            port_offset if port_offset is not None else 0,
        )

    base_url = (
        explicit
        if explicit is not None
        else f"http://127.0.0.1:{OS_SERVER_PRODUCTION_DEFAULT_PORT}"
    )
    return OsVerifyOptions(base_url, [], 0)


def launch_ports_for_offset(offset: int) -> list[int]:
    """Mirror launchPortsForOffset: table ports plus the dev OS-Server port."""
    return [port + offset for port in DEV_PORTS] + [DEV_OS_SERVER_PORT + offset]


def uses_packaged_os_server_port(offset: int) -> bool:
    return OS_SERVER_PRODUCTION_DEFAULT_PORT in launch_ports_for_offset(offset)


def auto_port_offset_candidates() -> list[int]:
    """Mirror autoPortOffsetCandidates, reserving the installed OS-Server port."""
    candidates = [0]
    for offset in range(
        DEV_PORT_NON_OVERLAP_SPAN,
        DEV_PORT_OFFSET_MAX + 1,
        DEV_PORT_NON_OVERLAP_SPAN,
    ):
        if not uses_packaged_os_server_port(offset):
            candidates.append(offset)
    return candidates


def is_tcp_port_in_use(port: int) -> bool:
    """Loopback TCP probe used by --port-offset auto."""
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.settimeout(0.2)
    try:
        probe.connect(("127.0.0.1", port))
        return True
    except OSError:
        return False
    finally:
        probe.close()


def resolve_auto_dev_port_offset(
    probe: Callable[[int], bool] | None = None,
) -> int:
    """Mirror resolveAutoDevPortOffset with an injectable probe."""
    probe_fn = probe or is_tcp_port_in_use
    for offset in auto_port_offset_candidates():
        for port in launch_ports_for_offset(offset):
            if probe_fn(port):
                break
        else:
            return offset
    raise OsTestConfigError("No free --port-offset auto candidate found")


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
        "--port-offset",
        default=None,
        help="Dev launch port offset <n|auto> (default: derived dev target)",
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

    # Port-offset parsing/validation precedes token IO and any spawn; auto
    # probing also precedes token IO. Ambient env is read only after --help.
    skip_start = args.skip_start
    try:
        spec = parse_wrapper_port_offset_arg(args.port_offset, skip_start)
        # Resolve the selected option (no file IO) before any auto TCP probe.
        token_file = resolve_token_file(
            target_args.token_file_flag,
            os.environ.get("FLOORP_OS_TOKEN_FILE"),
        )
        port_offset = (
            resolve_auto_dev_port_offset() if spec == "auto" else spec
        )
        resolved = resolve_os_verify_options(
            target_args.base_url_flag,
            os.environ.get("FLOORP_OS_BASE_URL"),
            port_offset,
            skip_start,
        )
    except OsTestConfigError as error:
        print(str(error), file=sys.stderr)
        return 2
    base_url = resolved.base_url
    try:
        # Reject an unsafe authenticated target before starting any browser. The
        # token file itself may still be published only after browser startup.
        if token_file is not None:
            validate_authenticated_base_url(base_url)
    except OsTestConfigError as error:
        print(str(error), file=sys.stderr)
        return 2

    timeout = max(1.0, args.timeout)
    interval = max(1.0, args.interval)
    cmd = (args.cmd if args.cmd else list(DEFAULT_CMD)) + resolved.start_command_args
    proc: subprocess.Popen[str] | None = None

    if skip_start:
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
