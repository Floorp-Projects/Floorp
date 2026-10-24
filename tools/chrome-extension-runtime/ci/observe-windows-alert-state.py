#!/usr/bin/env python3
"""Read actual Windows notification/session/desktop state without changing it.

This is a diagnostic observation, not a browser test or a notification policy
workaround. In particular it does not set Gecko's security/automation preferences,
unlock a session, change focus, or alter Windows notification settings.
"""

import argparse
import ctypes
from ctypes import wintypes
import datetime
import json
import os
from pathlib import Path
import platform
import sys


NOTIFICATION_STATES = {
    1: "QUNS_NOT_PRESENT",
    2: "QUNS_BUSY",
    3: "QUNS_RUNNING_D3D_FULL_SCREEN",
    4: "QUNS_PRESENTATION_MODE",
    5: "QUNS_ACCEPTS_NOTIFICATIONS",
    6: "QUNS_QUIET_TIME",
    7: "QUNS_APP",
}


def observe():
    if sys.platform != "win32":
        raise RuntimeError("This observer requires actual Windows native APIs")
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    shell32 = ctypes.WinDLL("shell32", use_last_error=True)

    shell32.SHQueryUserNotificationState.argtypes = [ctypes.POINTER(ctypes.c_int)]
    shell32.SHQueryUserNotificationState.restype = ctypes.c_long
    state = ctypes.c_int(-1)
    hresult = shell32.SHQueryUserNotificationState(ctypes.byref(state))

    kernel32.ProcessIdToSessionId.argtypes = [wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    kernel32.ProcessIdToSessionId.restype = wintypes.BOOL
    session_id = wintypes.DWORD()
    session_ok = bool(kernel32.ProcessIdToSessionId(os.getpid(), ctypes.byref(session_id)))
    session_error = ctypes.get_last_error() if not session_ok else None
    kernel32.WTSGetActiveConsoleSessionId.argtypes = []
    kernel32.WTSGetActiveConsoleSessionId.restype = wintypes.DWORD
    active_session = kernel32.WTSGetActiveConsoleSessionId()
    kernel32.GetCurrentThreadId.argtypes = []
    kernel32.GetCurrentThreadId.restype = wintypes.DWORD

    user32.GetProcessWindowStation.argtypes = []
    user32.GetProcessWindowStation.restype = wintypes.HANDLE
    user32.GetThreadDesktop.argtypes = [wintypes.DWORD]
    user32.GetThreadDesktop.restype = wintypes.HANDLE
    user32.OpenInputDesktop.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    user32.OpenInputDesktop.restype = wintypes.HANDLE
    user32.CloseDesktop.argtypes = [wintypes.HANDLE]
    user32.CloseDesktop.restype = wintypes.BOOL
    user32.GetUserObjectInformationW.argtypes = [
        wintypes.HANDLE, ctypes.c_int, wintypes.LPVOID,
        wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
    ]
    user32.GetUserObjectInformationW.restype = wintypes.BOOL

    def object_name(handle, open_error=None):
        if not handle:
            return {"name": None, "win32Error": open_error}
        required = wintypes.DWORD()
        # UOI_NAME=2. The first call discovers the exact native buffer size.
        ctypes.set_last_error(0)
        ok = user32.GetUserObjectInformationW(handle, 2, None, 0, ctypes.byref(required))
        error = ctypes.get_last_error()
        if not ok and error != 122:  # ERROR_INSUFFICIENT_BUFFER
            return {"name": None, "win32Error": error}
        if not 0 < required.value <= 65536:
            return {"name": None, "win32Error": error, "requiredBytes": required.value}
        buffer = ctypes.create_unicode_buffer(
            (required.value + ctypes.sizeof(ctypes.c_wchar) - 1) // ctypes.sizeof(ctypes.c_wchar)
        )
        ok = user32.GetUserObjectInformationW(
            handle, 2, buffer, ctypes.sizeof(buffer), ctypes.byref(required)
        )
        return {"name": buffer.value if ok else None,
                "win32Error": None if ok else ctypes.get_last_error()}

    station = user32.GetProcessWindowStation()
    station_error = ctypes.get_last_error() if not station else None
    desktop = user32.GetThreadDesktop(kernel32.GetCurrentThreadId())
    desktop_error = ctypes.get_last_error() if not desktop else None
    # DESKTOP_READOBJECTS only. No switching, input injection or permissions write.
    input_desktop = user32.OpenInputDesktop(0, False, 0x0001)
    input_error = ctypes.get_last_error() if not input_desktop else None
    try:
        input_observation = object_name(input_desktop, input_error)
    finally:
        if input_desktop:
            input_observation["closeSucceeded"] = bool(user32.CloseDesktop(input_desktop))
            if not input_observation["closeSucceeded"]:
                input_observation["closeWin32Error"] = ctypes.get_last_error()

    return {
        "scope": "actual Windows read-only notification/session/desktop observation",
        "observedAtUtc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "os": platform.platform(), "machine": platform.machine(),
        "processId": os.getpid(),
        "notificationState": {
            "hresult": f"0x{hresult & 0xffffffff:08x}",
            "querySucceeded": hresult >= 0,
            "value": state.value if hresult >= 0 else None,
            "name": NOTIFICATION_STATES.get(state.value) if hresult >= 0 else None,
            "acceptsNotifications": state.value == 5 if hresult >= 0 else None,
        },
        "session": {"processSessionId": session_id.value if session_ok else None,
                    "processSessionWin32Error": session_error,
                    "activeConsoleSessionId": None if active_session == 0xffffffff else active_session},
        "windowStation": object_name(station, station_error),
        "threadDesktop": object_name(desktop, desktop_error),
        "inputDesktop": input_observation,
        "browserLaunched": False, "settingsChanged": False,
        "nativeNotificationQualified": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Actual Windows is required; no synthetic platform result is emitted")
    if args.output.exists() or args.output.is_symlink():
        parser.error("Output must be a new owned file")
    report = observe()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(json.dumps({"report": str(args.output.resolve()),
                      "querySucceeded": report["notificationState"]["querySucceeded"],
                      "nativeNotificationQualified": False}))
    return 0 if report["notificationState"]["querySucceeded"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
