#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exercise a built Gecko host and production Shim with an isolated profile."""

import argparse
import ctypes
import http.server
import json
import os
from pathlib import Path
import plistlib
import select
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import uuid


class Marionette:
    def __init__(self, port, *, bidi=False):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=5)
        self.socket.settimeout(60)
        self.buffer = b""
        self.sequence = 0
        if self.receive().get("marionetteProtocol") != 3:
            raise RuntimeError("Expected Marionette protocol 3")
        capabilities = {"webSocketUrl": True,
                        "unhandledPromptBehavior": {"beforeUnload": "ignore"}} if bidi else {}
        self.session = self.command("WebDriver:NewSession", capabilities)
        if bidi and not isinstance(self.session.get("capabilities", {}).get("webSocketUrl"), str):
            self.close()
            raise RuntimeError("Browser did not create the requested WebDriver BiDi session")

    def receive(self):
        while True:
            if b":" in self.buffer:
                prefix, body = self.buffer.split(b":", 1)
                length = int(prefix)
                if len(body) >= length:
                    self.buffer = body[length:]
                    return json.loads(body[:length])
            packet = self.socket.recv(65536)
            if not packet:
                raise RuntimeError("Marionette connection closed")
            self.buffer += packet

    def command(self, name, parameters=None):
        self.sequence += 1
        body = json.dumps([0, self.sequence, name, parameters or {}]).encode()
        self.socket.sendall(str(len(body)).encode() + b":" + body)
        response = self.receive()
        if not isinstance(response, list) or response[:2] != [1, self.sequence]:
            raise RuntimeError(f"Unexpected Marionette response: {response}")
        if response[2]:
            raise RuntimeError(f"{name}: {response[2]}")
        return response[3]

    def context(self, value):
        self.command("Marionette:SetContext", {"value": value})

    def script(self, source, *args):
        result = self.command("WebDriver:ExecuteScript", {"script": source, "args": args})
        return result.get("value") if isinstance(result, dict) else result

    def navigate(self, url):
        self.command("WebDriver:Navigate", {"url": url})

    def switch(self, handle):
        self.command("WebDriver:SwitchToWindow", {"handle": handle, "focus": False})

    def handles(self):
        response = self.command("WebDriver:GetWindowHandles")
        return response["value"] if isinstance(response, dict) else response

    def close(self):
        self.socket.settimeout(5)
        try:
            self.command("WebDriver:DeleteSession")
        except (OSError, RuntimeError):
            pass
        self.socket.close()


class HostProcess:
    """Keep a LaunchServices helper distinct from the exact test browser it owns."""

    def __init__(self, browser, binary, profile, output, log, launch_services=False, *, bidi=False):
        self.profile = profile
        self.command = [str(binary), "--no-remote", "--profile", str(profile),
                        "--marionette", "--remote-allow-system-access", "about:blank"]
        if bidi:
            self.command[-1:-1] = ["--remote-debugging-port", "0"]
        self.launch_services = launch_services
        self.pid = None
        self.identity = None
        self.exited = False
        self.verified = False
        self.exit_events = None
        self.exit_code = None
        self.requested_signal = None
        command = self.command
        if launch_services:
            # open(1) owns the foreground request and keeps separate stdout and
            # stderr files; opening both streams at one path could overwrite data.
            command = ["/usr/bin/open", "-n", "-W", "-a", str(browser),
                       "--stdout", str(output / "browser.stdout.log"),
                       "--stderr", str(output / "browser.stderr.log"),
                       "--env", "MOZ_CRASHREPORTER_DISABLE=1", "--args",
                       *self.command[1:]]
        self.process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
            env={**os.environ, "MOZ_CRASHREPORTER_DISABLE": "1"})
        if not launch_services:
            self.pid = self.process.pid

    @staticmethod
    def _process_rows(pid=None):
        command = ["/bin/ps", "-ww"]
        command += ["-p", str(pid)] if pid is not None else ["-ax"]
        result = subprocess.run(command + ["-o", "pid=,lstart=,command="],
            capture_output=True, text=True, env={**os.environ, "LC_ALL": "C"}, timeout=5)
        if result.returncode not in (0, 1):
            raise RuntimeError(f"Cannot verify test browser process: {result.stderr.strip()}")
        rows = []
        for line in result.stdout.splitlines():
            fields = line.split(None, 6)
            if len(fields) == 7 and fields[0].isdigit():
                rows.append((int(fields[0]), " ".join(fields[1:6]), fields[6]))
        return rows

    def _matching_identity(self, pid):
        expected = " ".join(self.command)
        return next((row for row in self._process_rows(pid)
                     if row[0] == pid and row[2] == expected), None)

    def verify_session(self, session):
        capabilities = session.get("capabilities", {})
        pid = capabilities.get("moz:processID")
        profile = capabilities.get("moz:profile")
        if type(pid) is not int or pid <= 0 or profile != str(self.profile):
            raise RuntimeError("Marionette did not identify the isolated test profile and browser PID")
        if not self.launch_services and pid != self.process.pid:
            raise RuntimeError("Marionette browser PID differs from the launched child")
        identity = self._matching_identity(pid)
        if not identity:
            raise RuntimeError("Browser PID does not have the exact executable and isolated profile command")
        self._adopt(identity)
        self.verified = True

    def _adopt(self, identity):
        self.pid, self.identity = identity[0], identity
        if self.launch_services and self.exit_events is None:
            self.exit_events = select.kqueue()
            # Darwin sys/event.h: NOTE_EXITSTATUS is valid for a child or a
            # process we may signal. Python does not export this Darwin flag.
            self.exit_events.control([select.kevent(self.pid,
                filter=select.KQ_FILTER_PROC,
                flags=select.KQ_EV_ADD | select.KQ_EV_ONESHOT,
                fflags=select.KQ_NOTE_EXIT | 0x04000000)], 0, 0)
            if self._matching_identity(self.pid) != identity:
                raise RuntimeError("Browser identity changed while installing exit monitoring")

    def _read_exit_events(self):
        if self.exit_events is None:
            return
        for event in self.exit_events.control(None, 1, 0):
            if event.ident == self.pid and event.fflags & select.KQ_NOTE_EXIT:
                self.exited = True
                if event.fflags & 0x04000000:
                    self.exit_code = os.waitstatus_to_exitcode(event.data & 0xffff)

    def _discover_for_cleanup(self):
        # A launch may fail before Marionette is ready. Only the newly-created,
        # private profile's complete argv identifies a process we may clean up.
        expected = " ".join(self.command)
        matches = [row for row in self._process_rows() if row[2] == expected]
        if len(matches) > 1:
            raise RuntimeError("Multiple processes claim the isolated test profile; refusing cleanup")
        if matches:
            self._adopt(matches[0])
        return bool(matches)

    @property
    def returncode(self):
        # LaunchServices reparents the browser. open's success is not its exit code.
        self._read_exit_events()
        return self.exit_code if self.launch_services else self.process.returncode

    def poll(self):
        if not self.launch_services:
            return self.process.poll()
        self._read_exit_events()
        if self.exited:
            return 0  # Liveness sentinel; never reported as the browser exit code.
        if self.pid is None:
            return self.process.poll()
        if self._matching_identity(self.pid) != self.identity:
            self.exited = True
            return 0
        return None

    def _signal(self, value):
        if not self.launch_services:
            self.process.send_signal(value)
            self.requested_signal = value
            return
        if self.exited:
            return
        if self.pid is None and not self._discover_for_cleanup():
            if self.process.poll() is None:
                raise RuntimeError("LaunchServices browser identity unavailable; no application was signalled")
            return
        # Recheck both argv and start time immediately before signalling. A reused
        # PID or a different profile must never become a cleanup target.
        if self._matching_identity(self.pid) != self.identity:
            self.exited = True
            return
        try:
            os.kill(self.pid, value)
            self.requested_signal = value
        except ProcessLookupError:
            self.exited = True

    def terminate(self):
        self._signal(signal.SIGTERM)

    def kill(self):
        self._signal(signal.SIGKILL)

    def wait(self, timeout):
        if not self.launch_services:
            return self.process.wait(timeout=timeout)
        deadline = time.monotonic() + timeout
        while self.poll() is None:
            if time.monotonic() >= deadline:
                raise subprocess.TimeoutExpired(self.command, timeout)
            time.sleep(0.1)
        return None

    def close(self):
        try:
            # Discover an unconnected browser even if open has already exited.
            if self.launch_services and self.pid is None:
                self._discover_for_cleanup()
            if self.poll() is None:
                self.terminate()
                try:
                    self.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    self.kill()
                    self.wait(timeout=5)
        finally:
            if self.launch_services:
                # This only reaps the helper; browser cleanup happens above.
                try:
                    self.process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.process.terminate()
                    self.process.wait(timeout=5)
            if self.exit_events is not None:
                self._read_exit_events()
                self.exit_events.close()
                self.exit_events = None


def wait_for(description, callback, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = callback()
        if result:
            return result
        time.sleep(0.15)
    raise RuntimeError(f"Timed out waiting for {description}")


def run(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout.strip()


def unused_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class Fixture(http.server.BaseHTTPRequestHandler):
    token = ""
    requests = []

    def do_GET(self):
        cookie = self.headers.get("Cookie", "")
        self.requests.append({"path": self.path, "cookie": cookie})
        authenticated = f"floorp_session={self.token}" in cookie
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        if self.path == "/login":
            self.send_header("Set-Cookie", f"floorp_session={self.token}; HttpOnly; SameSite=Lax; Path=/")
        self.end_headers()
        body = """<!doctype html><meta charset=utf-8><title>Floorp App Shim Runtime Test</title>
<style>body{background:#16324f;color:white;font:24px system-ui;padding:48px}
input{font:24px system-ui;width:75%%}.tile{height:80px;width:160px;background:#56c596;
transform:translate(50px,30px) rotate(12deg);border-radius:20px}</style>
<h1>Native App Shim integration</h1><input id=editor autofocus aria-label=Editor>
<div class=tile></div><script>
document.body.dataset.authenticated=%s;
document.body.dataset.receivedCookie=%s;
document.body.dataset.sharedStorage=localStorage.getItem('floorp-shim-smoke') || '';
document.querySelector('#editor').addEventListener('input', e => {
 document.body.dataset.inputValue=e.target.value;
 document.cookie='floorp_shim_input='+encodeURIComponent(e.target.value)+'; SameSite=Lax; Path=/';
});
</script>""" % (json.dumps(str(authenticated).lower()), json.dumps(cookie))
        self.wfile.write(body.encode())

    def log_message(self, *_args):
        pass


def seal_bundle(directory, executable, identity, profile, app_id, profile_id):
    bundle = directory / "Runtime Smoke.app"
    macos = bundle / "Contents/MacOS"
    macos.mkdir(parents=True, mode=0o700)
    bundle.chmod(0o700)
    shutil.copy2(executable, macos / "floorp-app-shim")
    (macos / "floorp-app-shim").chmod(0o755)
    metadata = {
        "CFBundleIdentifier": "org.floorp.appshim.smoke." + uuid.uuid4().hex,
        "CFBundleDisplayName": "Floorp App Shim Runtime Test",
        "CFBundleName": "Floorp App Shim Runtime Test",
        "CFBundleExecutable": "floorp-app-shim",
        "CFBundlePackageType": "APPL",
        "CFBundleVersion": "1",
        "NSPrincipalClass": "NSApplication",
        "NSHighResolutionCapable": True,
        "FloorpAppShimAppId": app_id,
        "FloorpAppShimProfileId": profile_id,
        "FloorpAppShimHostBundleIdentifier": identity["identifier"],
        "FloorpAppShimHostCodeRequirement": identity["designatedRequirement"],
        "FloorpAppShimHostPath": identity["bundlePath"],
        "FloorpAppShimProfilePath": str(profile),
    }
    with (bundle / "Contents/Info.plist").open("wb") as stream:
        plistlib.dump(metadata, stream)
    run("/usr/bin/codesign", "--force", "--sign", "-", str(bundle))
    run("/usr/bin/codesign", "--verify", "--strict", str(bundle))
    return bundle


def native_type(pid, text):
    graphics = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    core = ctypes.CDLL("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")
    graphics.CGPreflightPostEventAccess.restype = ctypes.c_bool
    if not graphics.CGPreflightPostEventAccess():
        raise RuntimeError("Native input requires existing macOS Accessibility permission for this test runner; no permission prompt was opened")
    graphics.CGEventCreateKeyboardEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_bool]
    graphics.CGEventCreateKeyboardEvent.restype = ctypes.c_void_p
    graphics.CGEventKeyboardSetUnicodeString.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_uint16)]
    graphics.CGEventSetFlags.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
    graphics.CGEventPostToPid.argtypes = [ctypes.c_int, ctypes.c_void_p]
    core.CFRelease.argtypes = [ctypes.c_void_p]
    for char in text:
        units = char.encode("utf-16-le")
        value = (ctypes.c_uint16 * (len(units) // 2)).from_buffer_copy(units)
        for down in (True, False):
            event = graphics.CGEventCreateKeyboardEvent(None, 0, down)
            if not event:
                raise RuntimeError("CGEventCreateKeyboardEvent failed")
            graphics.CGEventKeyboardSetUnicodeString(event, len(value), value)
            graphics.CGEventSetFlags(event, 0)
            graphics.CGEventPostToPid(pid, event)
            core.CFRelease(event)
        time.sleep(0.035)


def native_window_ids(pid):
    graphics = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    core = ctypes.CDLL("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")
    graphics.CGWindowListCopyWindowInfo.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    graphics.CGWindowListCopyWindowInfo.restype = ctypes.c_void_p
    core.CFArrayGetCount.argtypes = [ctypes.c_void_p]
    core.CFArrayGetCount.restype = ctypes.c_long
    core.CFArrayGetValueAtIndex.argtypes = [ctypes.c_void_p, ctypes.c_long]
    core.CFArrayGetValueAtIndex.restype = ctypes.c_void_p
    core.CFDictionaryGetValue.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    core.CFDictionaryGetValue.restype = ctypes.c_void_p
    core.CFNumberGetValue.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
    core.CFNumberGetValue.restype = ctypes.c_bool
    core.CFRelease.argtypes = [ctypes.c_void_p]
    keys = {name: ctypes.c_void_p.in_dll(graphics, "kCGWindow" + name) for name in ("OwnerPID", "Layer", "Number")}
    windows = graphics.CGWindowListCopyWindowInfo(1, 0)
    if not windows:
        return []
    result = []
    try:
        for index in range(core.CFArrayGetCount(windows)):
            window = core.CFArrayGetValueAtIndex(windows, index)
            values = {}
            for name, key in keys.items():
                number = core.CFDictionaryGetValue(window, key)
                value = ctypes.c_int32()
                if number and core.CFNumberGetValue(number, 3, ctypes.byref(value)):
                    values[name] = value.value
            if values.get("OwnerPID") == pid and values.get("Layer") == 0:
                result.append(values["Number"])
    finally:
        core.CFRelease(windows)
    return result


def capture_fixture_window(pid, window_id, output):
    graphics = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    graphics.CGPreflightScreenCaptureAccess.restype = ctypes.c_bool
    if not graphics.CGPreflightScreenCaptureAccess():
        return {"status": "unavailable", "reason": "Screen Recording permission is not already granted; no prompt opened"}
    if window_id not in native_window_ids(pid):
        return {"status": "unavailable", "reason": "Verified Shim window is no longer visible"}
    output.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(["/usr/sbin/screencapture", "-x", "-o", "-l", str(window_id), str(output)], text=True, capture_output=True, timeout=15)
    if result.returncode or not output.is_file():
        return {"status": "unavailable", "reason": result.stderr.strip() or "Window capture failed"}
    return {"status": "captured", "path": str(output), "windowId": window_id, "ownerPid": pid,
            "visualAssessment": "pending image inspection"}


def native_quit(pid):
    """Send Cmd+Q through AppKit's menu routing, not the host service API."""
    graphics = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    core = ctypes.CDLL("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")
    graphics.CGPreflightPostEventAccess.restype = ctypes.c_bool
    if not graphics.CGPreflightPostEventAccess():
        raise RuntimeError("Existing macOS event-posting permission is required")
    graphics.CGEventCreateKeyboardEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_bool]
    graphics.CGEventCreateKeyboardEvent.restype = ctypes.c_void_p
    graphics.CGEventSetFlags.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
    graphics.CGEventPostToPid.argtypes = [ctypes.c_int, ctypes.c_void_p]
    core.CFRelease.argtypes = [ctypes.c_void_p]
    for down in (True, False):
        event = graphics.CGEventCreateKeyboardEvent(None, 12, down)
        if not event:
            raise RuntimeError("Cannot create Cmd+Q event")
        graphics.CGEventSetFlags(event, 1 << 20)
        graphics.CGEventPostToPid(pid, event)
        core.CFRelease(event)


STATE = "Services.appShell.hiddenDOMWindow.__floorpAppShimSmoke"
SERVICE = 'Cc["@floorp.org/mac-web-app-service;1"].getService(Ci.nsIMacWebAppService)'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path, help="Built signed browser .app (never a user's installed browser)")
    parser.add_argument("--shim", type=Path, help="Override the matching Shim packaged inside --browser")
    parser.add_argument("--output", type=Path, default=Path("_dist/app-shim-runtime-smoke"))
    parser.add_argument("--timeout", type=float, default=90)
    parser.add_argument("--launch-services", action="store_true", help="Launch the exact built app through macOS LaunchServices; verify the real browser PID separately from open")
    parser.add_argument("--capture-window", nargs="?", const=Path("fixture-window.png"), type=Path, help="Capture only the verified fixture window with existing Screen Recording permission; optional output path")
    parser.add_argument("--skip-native-input", action="store_true", help="Collect partial evidence only; exits 2, never reports a pass")
    parser.add_argument("--frontend-modules", type=Path, help="Also exercise the real installer and native lifecycle using this bundled module directory")
    parser.add_argument("--quit-all", action="store_true", help="With frontend modules, test the setting that quits the browser and apps together")
    args = parser.parse_args()
    if args.quit_all and not args.frontend_modules:
        parser.error("--quit-all requires --frontend-modules")
    browser = args.browser.resolve(strict=True)
    shim = (args.shim or browser / "Contents/MacOS/floorp-app-shim").resolve(strict=True)
    if sys.platform != "darwin" or browser.suffix != ".app":
        parser.error("This test requires macOS and a built browser .app")
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    profile = output / "profile"
    profile.mkdir(mode=0o700)
    with (browser / "Contents/Info.plist").open("rb") as stream:
        binary = browser / "Contents/MacOS" / plistlib.load(stream)["CFBundleExecutable"]
    report = {"status": "failed", "phase": "host-signature", "browser": str(browser), "output": str(output)}
    host = client = server = None
    log = (output / "browser.log").open("w")
    events = []
    exit_code = 1
    try:
        run("/usr/bin/codesign", "--verify", "--strict", str(browser))
        port = unused_port()
        preferences = {
            "marionette.port": port,
            "marionette.log.level": "Debug",
            "browser.shell.checkDefaultBrowser": False,
            "browser.startup.homepage_override.mstone": "ignore",
            "browser.startup.page": 0,
            "browser.startup.homepage": "about:blank",
            "browser.aboutwelcome.enabled": False,
            "browser.sessionstore.resume_from_crash": False,
            "browser.warnOnQuit": False,
            "browser.tabs.warnOnClose": False,
            "app.update.enabled": False,
            "datareporting.policy.dataSubmissionEnabled": False,
        }
        (profile / "user.js").write_text("\n".join(f"user_pref({json.dumps(key)}, {json.dumps(value)});" for key, value in preferences.items()) + "\n")
        report["phase"] = "launch-host"
        # Classic-only Marionette automatically accepts beforeunload prompts.
        # A real BiDi session preserves them for the explicit veto assertion.
        bidi = bool(args.frontend_modules)
        host = HostProcess(browser, binary, profile, output, log, args.launch_services, bidi=bidi)
        report["launchMode"] = "launch-services" if args.launch_services else "subprocess"
        if args.launch_services:
            report["launcherPid"] = host.process.pid
            report["browserLogs"] = [str(output / "browser.stdout.log"), str(output / "browser.stderr.log")]

        def connect():
            if host.poll() is not None:
                raise RuntimeError(f"Host exited {host.returncode}; see browser.log")
            try:
                return Marionette(port, bidi=bidi)
            except (ConnectionRefusedError, socket.timeout):
                return None

        client = wait_for("Marionette", connect, args.timeout)
        host.verify_session(client.session)
        report["hostPid"] = host.pid
        report["hostProcessIdentity"] = {"pid": host.pid, "startTime": host.identity[1], "command": host.identity[2]}
        report["marionetteSession"] = client.session
        client.context("chrome")
        report["phase"] = "native-service"
        identity = json.loads(client.script(f"return {SERVICE}.hostIdentityJSON;"))
        report["hostIdentity"] = identity
        report["capabilities"] = json.loads(client.script(f"return {SERVICE}.capabilitiesJSON;"))
        app_id, profile_id = "{" + str(uuid.uuid4()) + "}", "smoke-" + uuid.uuid4().hex
        client.script(f"""
            {STATE} = {{events: [], appId: arguments[0], normalWindow: window}};
            const state = {STATE};
            state.observer = {{observe(subject, topic, data) {{ state.events.push(JSON.parse(data)); }}}};
            Services.obs.addObserver(state.observer, 'floorp-web-app-shim-event');
            return true;
        """, app_id)
        if args.frontend_modules:
            modules = args.frontend_modules.resolve(strict=True)
            client.script(f"""
                const directory = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
                directory.initWithPath(arguments[0]);
                Services.io.getProtocolHandler('resource').QueryInterface(Ci.nsIResProtocolHandler)
                    .setSubstitution('noraneko', Services.io.newFileURI(directory));
                Services.prefs.setBoolPref('floorp.browser.nativeApp.appShim.enabled', true);
                Services.prefs.setBoolPref('floorp.browser.nativeApp.keepRunningAfterBrowserQuit', true);
                const {{MacNativeAppRuntime}} = ChromeUtils.importESModule('resource://noraneko/modules/pwa/NativeAppRuntime.sys.mjs');
                {STATE}.runtime = new MacNativeAppRuntime({{
                    profileDirectory:arguments[1], applicationsDirectory:arguments[2],
                    browserExecutable:arguments[3], shimExecutable:arguments[4]
                }});
            """, str(modules), str(profile), str(output / "Applications"), str(binary), str(shim))
        else:
            bundle = seal_bundle(output, shim, identity, profile, app_id, profile_id)
            client.script(f"{SERVICE}.configure(arguments[0]); {SERVICE}.registerApp(arguments[1], arguments[2]);", profile_id, app_id, str(bundle))

        Fixture.token = uuid.uuid4().hex
        Fixture.requests = []
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        base = f"http://127.0.0.1:{server.server_port}"
        client.context("content")
        normal_handle = client.command("WebDriver:GetWindowHandle")["value"]
        client.navigate(base + "/login")
        client.script("localStorage.setItem('floorp-shim-smoke', arguments[0]);", Fixture.token)
        client.context("chrome")
        client.script("Cc['@mozilla.org/widget/macdocksupport;1'].getService(Ci.nsIMacDockSupport).activateApplication(true);")
        time.sleep(0.3)
        handles_before = set(client.handles())
        host_windows_before = set(native_window_ids(host.pid))
        if args.frontend_modules:
            client.script(f"""
                const {{BrowserWindowTracker}} = ChromeUtils.importESModule('resource:///modules/BrowserWindowTracker.sys.mjs');
                const state = {STATE};
                state.manifest = {{id:arguments[0], name:'Runtime Smoke', start_url:arguments[1], icon:'', userContextId:0}};
                state.createWindow = () => {{
                    const args = Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);
                    args.data = state.manifest.start_url;
                    return BrowserWindowTracker.openWindow({{args, features:'width=900,height=700', remote:true, fission:true}});
                }};
                state.launch = () => {{
                    state.launchDone = false;
                    state.launchError = null;
                    state.runtime.open(state.manifest, state.createWindow).then(win => {{
                        state.window = win;
                        state.launchDone = true;
                        if (!win) state.launchError = 'Native backend did not complete launch';
                    }}, error => {{state.launchDone = true; state.launchError = String(error);}});
                }};
                state.launch();
            """, app_id, base + "/app")
        else:
            client.script(f"{SERVICE}.launchApp(arguments[0]);", app_id)
        report["phase"] = "authenticated-shim"

        def current_events():
            nonlocal events
            client.context("chrome")
            events = client.script(f"return {STATE}.events;")
            disconnected = next((event for event in events if event["type"] == "disconnected"), None)
            if disconnected:
                raise RuntimeError(f"Shim disconnected: {disconnected}")
            if args.frontend_modules:
                error = client.script(f"return {STATE}.launchError;")
                if error:
                    raise RuntimeError(error)
            return events

        connected = wait_for("authenticated Shim", lambda: next((event for event in current_events() if event["type"] == "connected"), None), args.timeout)
        shim_pid = connected["payload"].get("pid")
        if not isinstance(shim_pid, int) or shim_pid <= 0 or shim_pid == host.pid:
            raise RuntimeError("Connected event did not identify a distinct authenticated Shim PID")
        report["shimPid"] = shim_pid
        report["phase"] = "gecko-window"
        if not args.frontend_modules:
            client.script(f"""
            const {{BrowserWindowTracker}} = ChromeUtils.importESModule('resource:///modules/BrowserWindowTracker.sys.mjs');
            const state = {STATE};
            const args = Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);
            args.data = arguments[1];
            {SERVICE}.withWindowContext(arguments[0], {{createWindow() {{
                state.window = BrowserWindowTracker.openWindow({{args, features:'width=900,height=700', remote:true, fission:true}});
                return state.window;
            }}}});
            return true;
            """, app_id, base + "/app")
        created = wait_for("native window creation", lambda: next((event for event in current_events() if event["type"] == 101 and event["payload"].get("kind") == "created"), None), args.timeout)
        report["window"] = created["payload"]
        report["nativeWindowIds"] = wait_for("WindowServer ownership by Shim", lambda: native_window_ids(shim_pid), args.timeout)
        frame = wait_for("actual Gecko compositor frame", lambda: next((event for event in current_events() if event["type"] == 111), None), args.timeout)
        report["firstFrame"] = frame
        unexpected_host_windows = set(native_window_ids(host.pid)) - host_windows_before
        if unexpected_host_windows:
            raise RuntimeError(f"Web App created native windows in the browser process: {unexpected_host_windows}")
        report["noBrowserOwnedProxyWindow"] = True
        if args.frontend_modules:
            wait_for("frontend launch and installation commit", lambda: client.script(f"return {STATE}.launchDone;"), args.timeout)
            error = client.script(f"return {STATE}.launchError;")
            if error:
                raise RuntimeError(error)
            registry = json.loads((profile / "ssb/app-registry.json").read_text())
            installed = next(app for app in registry["apps"] if app["installId"] == app_id)
            if installed["integration"] != "app-shim" or not installed.get("installedShim"):
                raise RuntimeError("Launch did not commit native installation")
            report["frontendInstallation"] = installed
        new_handle = wait_for("Shim browsing context", lambda: next(iter(set(client.handles()) - handles_before), None), args.timeout)
        client.switch(new_handle)
        client.context("content")
        report["phase"] = "shared-session"
        page = wait_for("authenticated app page", lambda: client.script("return document.querySelector('#editor') ? {authenticated:document.body.dataset.authenticated, sharedStorage:document.body.dataset.sharedStorage, cookie:document.cookie} : null;"), args.timeout)
        if page["authenticated"] != "true" or page["sharedStorage"] != Fixture.token:
            raise RuntimeError(f"Browser session did not reach Shim page: {page}")
        if "floorp_session=" in page["cookie"]:
            raise RuntimeError("HttpOnly cookie unexpectedly visible to content")
        report["sharedSession"] = page
        client.context("chrome")
        def expected_launch_geometry():
            states = [event["payload"] for event in current_events()
                      if event["type"] == 101 and event["payload"].get("windowId") == created["payload"]["windowId"]]
            latest = states[-1] if states else None
            return latest if latest and latest["width"] == 900 and latest["height"] == 700 else None

        report["defaultLaunchGeometry"] = wait_for("requested default 900x700 native window", expected_launch_geometry, args.timeout)
        frames_before = sum(event["type"] == 111 for event in current_events())
        client.context("content")
        client.script("document.body.style.backgroundColor='#295f4f';")
        report["contentRepaint"] = wait_for("content repaint in native Shim", lambda: sum(event["type"] == 111 for event in current_events()) > frames_before, args.timeout)
        if args.capture_window:
            capture_path = args.capture_window if args.capture_window.is_absolute() else output / args.capture_window
            try:
                client.context("chrome")
                client.script(f"{STATE}.window.resizeTo(900, 700); {SERVICE}.sendControl(arguments[0], 13, JSON.stringify({{windowId:arguments[1]}}));", app_id, created["payload"]["windowId"])
                wait_for("active fixture window at capture size", lambda: any(
                    event["type"] == 101 and event["payload"].get("key") and
                    event["payload"].get("width", 0) >= 800 and event["payload"].get("height", 0) >= 600
                    for event in current_events()), args.timeout)
                # WindowServer may animate activation from Stage Manager's strip.
                # Wait after the native geometry/focus acknowledgement so captures
                # show the actual fixture instead of an in-flight desktop animation.
                time.sleep(0.8)
                report["windowCapture"] = capture_fixture_window(shim_pid, report["nativeWindowIds"][0], capture_path)
            except (OSError, subprocess.TimeoutExpired, RuntimeError) as error:
                report["windowCapture"] = {"status": "unavailable", "reason": str(error)}
        client.context("content")
        client.script("document.querySelector('#editor').focus();")
        if args.skip_native_input:
            report["status"] = "partial"
            report["nativeInput"] = "explicitly skipped"
            exit_code = 2
        else:
            report["phase"] = "native-input"
            client.context("chrome")
            client.script(f"{SERVICE}.sendControl(arguments[0], 13, JSON.stringify({{windowId:arguments[1]}}));", app_id, created["payload"]["windowId"])
            time.sleep(0.3)
            input_text = "shim-" + uuid.uuid4().hex[:8]
            native_type(shim_pid, input_text)
            client.context("content")
            value = wait_for("native Shim text reaching Gecko", lambda: client.script("return document.querySelector('#editor').value;") == input_text, args.timeout)
            report["nativeInput"] = {"method": "CGEventPostToPid -> AppKit -> authenticated Mach -> Gecko", "matched": value}
            client.context("chrome")
            input_events = [event for event in current_events() if event["type"] == 100]
            if not input_events:
                raise RuntimeError("Text changed without an authenticated Shim input event")
            report["nativeInput"]["eventCount"] = len(input_events)
            client.switch(normal_handle)
            client.context("content")
            client.navigate(base + "/return")
            returned = client.script("return document.body.dataset.receivedCookie;")
            if f"floorp_shim_input={input_text}" not in returned:
                raise RuntimeError("Cookie set by Shim input was not returned by normal browser")
            report["reverseCookieSharing"] = True
            if not args.frontend_modules:
                report["phase"] = "window-close-retirement"
                client.context("chrome")
                client.script(f"{STATE}.events = []; {STATE}.window.close();")
                window_id = created["payload"]["windowId"]
                closed = wait_for("native window close acknowledgement", lambda: client.script(
                    f"return {STATE}.events.find(e => e.type === 101 && e.payload.kind === 'closed' && e.payload.windowId === arguments[0]) || null;",
                    window_id), args.timeout)
                if native_window_ids(shim_pid):
                    raise RuntimeError("Native close acknowledgement preceded window disappearance")
                # A correctly acknowledged close releases its lease record;
                # the watchdog must not subsequently kill an otherwise live app.
                deadline = time.monotonic() + 5.5
                while time.monotonic() < deadline:
                    current_events()
                    time.sleep(0.1)
                report["windowCloseRetirement"] = {"acknowledged": closed, "peerSurvivedWatchdog": True}
            if args.frontend_modules:
                report["phase"] = "native-process-recovery"
                client.switch(new_handle)
                client.context("chrome")
                client.script(f"{STATE}.events = [];")
                previous_pid = shim_pid
                os.kill(previous_pid, signal.SIGKILL)
                recovered = wait_for("replacement authenticated Shim", lambda: client.script(f"return {STATE}.events.find(e => e.type === 'connected') || null;"), args.timeout)
                shim_pid = recovered["payload"]["pid"]
                if shim_pid == previous_pid:
                    raise RuntimeError("Recovery reused the dead native process")
                wait_for("recreated native window and compositor frame", lambda: native_window_ids(shim_pid) and client.script(f"return {STATE}.events.some(e => e.type === 111);"), args.timeout)
                client.context("content")
                if client.script("return document.querySelector('#editor').value;") != input_text:
                    raise RuntimeError("Native process recovery replaced the live Gecko page or lost form state")
                report["nativeProcessRecovery"] = {"previousPid": previous_pid, "replacementPid": shim_pid, "preservedPageAndInput": True}
                report["phase"] = "beforeunload-veto"
                report["beforeunloadState"] = client.script("""
                    localStorage.removeItem('floorp-shim-beforeunload-fired');
                    window.onbeforeunload = event => {
                        localStorage.setItem('floorp-shim-beforeunload-fired', 'true');
                        event.preventDefault();
                        event.returnValue = '';
                    };
                    return {handlerInstalled: typeof window.onbeforeunload === 'function',
                        hasBeenActive: navigator.userActivation.hasBeenActive,
                        isActive: navigator.userActivation.isActive,
                        inputValue: document.querySelector('#editor').value};
                """)
                native_quit(shim_pid)

                def pending_alert():
                    try:
                        return client.command("WebDriver:GetAlertText")
                    except RuntimeError as error:
                        if "no such alert" in str(error).lower():
                            return None
                        if "no such window" in str(error).lower():
                            client.switch(normal_handle)
                            client.context("chrome")
                            report["beforeunloadCloseState"] = client.script(f"""
                                return {{appClosed: {STATE}.window.closed,
                                    browserClosed: {STATE}.normalWindow.closed,
                                    events: {STATE}.events,
                                    beforeunloadDisabled: Services.prefs.getBoolPref('dom.disable_beforeunload', false),
                                    requiresInteraction: Services.prefs.getBoolPref('dom.require_user_interaction_for_beforeunload', true)}};
                            """)
                            client.context("content")
                            report["beforeunloadCloseState"]["handlerFired"] = client.script(
                                "return localStorage.getItem('floorp-shim-beforeunload-fired');")
                        raise

                wait_for("beforeunload prompt from native Cmd+Q", pending_alert, args.timeout)
                client.command("WebDriver:DismissAlert")
                if client.script("return document.querySelector('#editor').value;") != input_text or not native_window_ids(shim_pid):
                    raise RuntimeError("Cancelling app quit lost the page or native window")
                client.script("window.onbeforeunload = null;")
                report["beforeunloadVetoKeepsApp"] = True
                client.switch(normal_handle)
                client.context("chrome")
                client.script(f"{STATE}.events = [];")
                report["phase"] = "app-quit"
                client.context("chrome")
                client.script(f"{STATE}.window.focus();")
                time.sleep(0.2)
                native_quit(shim_pid)
                wait_for("app-specific Cmd+Q", lambda: client.script(f"return {STATE}.window.closed && {STATE}.events.some(e => e.type === 'disconnected');"), args.timeout)
                if host.poll() is not None or client.script(f"return {STATE}.normalWindow.closed;"):
                    raise RuntimeError("App Cmd+Q terminated the normal browser")
                report["appQuitKeepsBrowser"] = True
                handles_before = set(client.handles())
                client.script(f"{STATE}.events = []; {STATE}.launch();")
                wait_for("reopen installed app", lambda: client.script(f"return {STATE}.launchDone;"), args.timeout)
                error = client.script(f"return {STATE}.launchError;")
                if error:
                    raise RuntimeError(error)
                connected = next(event for event in client.script(f"return {STATE}.events;") if event["type"] == "connected")
                shim_pid = connected["payload"]["pid"]
                new_handle = next(iter(set(client.handles()) - handles_before))
                client.switch(new_handle)
                client.context("chrome")
                if args.quit_all:
                    report["phase"] = "browser-quit-all"
                    client.script(f"Services.prefs.setBoolPref('floorp.browser.nativeApp.keepRunningAfterBrowserQuit', false); {STATE}.normalWindow.focus();")
                    events = client.script(f"return {STATE}.events;")
                    time.sleep(0.2)
                    native_quit(host.pid)
                    wait_for("global quit with setting disabled", lambda: host.poll() is not None and not native_window_ids(shim_pid), args.timeout)
                    report["browserQuitAllClosesApp"] = True
                else:
                    report["phase"] = "browser-quit-with-app-running"
                    client.script(f"{STATE}.normalWindow.goQuitApplication();")
                    wait_for("browser-only quit", lambda: client.script(f"return {STATE}.normalWindow.closed && !{STATE}.window.closed;"), args.timeout)
                    if host.poll() is not None or not native_window_ids(shim_pid):
                        raise RuntimeError("Browser-only quit lost the shared runtime or native app")
                    report["browserQuitKeepsApp"] = True
                    events = client.script(f"return {STATE}.events;")
                    client.script(f"{STATE}.window.focus();")
                    time.sleep(0.2)
                    native_quit(shim_pid)
                    wait_for("idle shared runtime exit after last app", lambda: host.poll() is not None, args.timeout)
                    report["lastAppQuitsIdleRuntime"] = True
            report["status"] = "passed"
            exit_code = 0
        report["phase"] = "complete"
    except Exception as error:
        report["error"] = str(error)
        print(f"Runtime smoke failed in {report['phase']}: {error}", file=sys.stderr)
    finally:
        if client and host and host.verified and host.poll() is None:
            try:
                client.context("chrome")
                events = client.script(f"return {STATE}?.events ?? [];")
                client.script(f"if ({STATE}?.window && !{STATE}.window.closed) {STATE}.window.close(); {SERVICE}.stop(); if ({STATE}?.observer) Services.obs.removeObserver({STATE}.observer, 'floorp-web-app-shim-event');")
            except Exception as error:
                report["cleanupError"] = str(error)
                report["status"] = "failed"
                exit_code = 1
        if client:
            if host and host.verified:
                client.close()
            else:
                client.socket.close()
        if host:
            try:
                host.close()
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                report["hostCleanupError"] = str(error)
                report["status"] = "failed"
                exit_code = 1
            report["hostExitCode"] = host.returncode
            if host.returncode not in (None, 0, -int(host.requested_signal or 0)):
                report["hostCleanupError"] = f"Browser exited abnormally: {host.returncode}"
                report["status"] = "failed"
                exit_code = 1
            if args.launch_services:
                report["launcherExitCode"] = host.process.returncode
                if host.returncode is None:
                    report["hostExitCodeUnavailable"] = "Browser exited before a kernel exit-status event was observed"
        if server:
            server.shutdown()
            server.server_close()
        log.close()
        report["events"] = events
        report["requests"] = Fixture.requests
        (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        print(f"{report['status']}: {output / 'report.json'}")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
