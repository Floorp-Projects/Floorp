# SPDX-License-Identifier: MPL-2.0
"""Bounded, allowlisted diagnostics for the isolated context-menu probe."""

import json
from pathlib import Path


REASONS = {
    "launch-failed", "process-exited", "process-monitor-failed", "send-failed",
    "not-running", "connection-generation-exhausted", "window-close-timeout",
}
ERROR_CODES = ("NS_ERROR_NOT_AVAILABLE", "NS_ERROR_FAILURE", "NS_ERROR_DOM_SECURITY_ERR",
               "NS_ERROR_NOT_INITIALIZED", "NS_ERROR_INVALID_ARG", "NS_ERROR_IN_PROGRESS")
LOG_MARKERS = {
    "App Shim requires --host-service SERVICE --host-pid PID --launch-token TOKEN.": "shim-invalid-arguments",
    "App Shim refused startup: signed bundle and pinned host identity are required.": "shim-startup-refused",
    "App Shim could not connect to its registered browser host.": "shim-host-lookup-failed",
    "Library not loaded:": "loader-library-not-loaded",
    "Bad CPU type in executable": "loader-unsupported-cpu",
    "Code Signature Invalid": "invalid-code-signature",
    "###!!! ASSERTION:": "gecko-assertion",
    "Marionette enabled": "marionette-enabled",
    "Listening on port": "listener-started",
}
WAIT_PHASES = {"Marionette", "Shim authentication", "Native PWA window", "PWA browsing context",
               "Fixture", "Trusted contextmenu delivery"}


def error_summary(error):
    text = str(error)
    result = {"type": type(error).__name__}
    for phase in WAIT_PHASES:
        if text == f"Timed out waiting for {phase}":
            result["timeout"] = phase
    result["codes"] = [code for code in ERROR_CODES if code in text]
    return result


def event_summary(events):
    result = []
    auth_count = 0
    for event in events:
        if not isinstance(event, dict) or event.get("type") not in ("connected", "disconnected"):
            continue
        row = {"type": event["type"]}
        stamp = event.get("diagnosticTime")
        if type(stamp) is int:
            row["timeMs"] = stamp
        payload = event.get("payload")
        if isinstance(payload, dict):
            pid = payload.get("pid")
            if type(pid) is int and pid > 0:
                row["pid"] = pid
            if event["type"] == "disconnected":
                reason = payload.get("reason")
                row["reason"] = reason if isinstance(reason, str) and reason in REASONS else "other"
        result.append(row)
        auth_count += 1
        if len(result) > 64:
            result.pop(0)
    return {"events": result, "observedEventCount": len(events), "authEventCount": auth_count,
            "truncated": auth_count > 64}


def process_summary(host):
    if host is None:
        return {"started": False}
    result = {"started": True, "browserPid": host.pid, "identityVerified": host.verified,
              "launchServices": host.launch_services, "requestedSignal": host.requested_signal}
    try:
        host.poll()
        result.update({"browserExited": host.exited, "browserExitCode": host.returncode,
                       "launcherExitCode": host.process.poll()})
    except Exception as error:
        result["observationError"] = error_summary(error)
    return result


def save_streams(private_dir, output):
    summaries = {}
    for name in ("browser.log", "browser.stdout.log", "browser.stderr.log"):
        path = Path(private_dir) / name
        summary = {"present": path.is_file(), "rawContentPublished": False, "markers": []}
        if summary["present"]:
            try:
                with path.open("rb") as stream:
                    size = stream.seek(0, 2)
                    offset = max(0, size - 65536)
                    stream.seek(offset)
                    data = stream.read(65536)
                lines = data.decode("utf-8", errors="replace").splitlines()
                if offset and lines:
                    lines.pop(0)
                markers = [marker for line in lines for text, marker in LOG_MARKERS.items() if text in line]
                codes = [code for line in lines for code in ERROR_CODES if code in line]
                summary.update({"bytesObserved": size, "tailBytesRead": len(data), "tailTruncated": offset > 0,
                                "linesObservedInTail": len(lines), "markers": markers[-64:], "codes": codes[-64:]})
            except OSError as error:
                summary["error"] = error_summary(error)
        (Path(output) / name).write_text(json.dumps(summary, indent=2) + "\n")
        summaries[name] = summary
    return summaries


def write_first_failure(output, report):
    try:
        with (Path(output) / "first-failure.json").open("x") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
    except FileExistsError:
        pass
