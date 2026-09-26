#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Test Finder-style warm and cold launch with a fully bundled Floorp frontend.

The initial installation uses the real native runtime with a disposable
Applications directory. Subsequent launches use LaunchServices and the normal
registered --start-ssb handler; no module substitution or handler injection is
performed. Supply a separate test copy of the browser, never an installed app.
"""

import argparse
import http.server
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import select
import signal
import socket
import subprocess
import tempfile
import threading
import time
import traceback
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("_dist/app-shim-cold-tests"))
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--inspect-seconds", type=int, default=0, choices=range(61),
                        metavar="0..60", help="Keep the verified cold window open briefly for manual inspection")
    args = parser.parse_args()
    browser = args.browser.resolve(strict=True)
    if browser.suffix != ".app" or not (
        browser / "Contents/Resources/noraneko/noraneko.manifest"
    ).is_file():
        parser.error("A separately prepared browser with its full Floorp frontend is required")
    spec = importlib.util.spec_from_file_location(
        "runtime_smoke", Path(__file__).with_name("test-runtime.py")
    )
    smoke = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(smoke)
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    profile = output / "profile"
    profile.mkdir(mode=0o700)
    unrelated_profile = output / "unrelated-profile"
    unrelated_local = output / "unrelated-local"
    unrelated_profile.mkdir(mode=0o700)
    unrelated_local.mkdir(mode=0o700)
    profile_environment = {
        "XRE_PROFILE_PATH": str(unrelated_profile),
        "XRE_PROFILE_LOCAL_PATH": str(unrelated_local),
        "SELECTABLE_PROFILE_RESET_PATH": str(unrelated_profile),
        "SELECTABLE_PROFILE_RESET_STORE_ID": "floorp-cold-test-unrelated-store",
    }
    with (browser / "Contents/Info.plist").open("rb") as stream:
        binary = browser / "Contents/MacOS" / plistlib.load(stream)["CFBundleExecutable"]
    app_id = "{" + str(uuid.uuid4()) + "}"
    token = uuid.uuid4().hex
    host = client = server = None
    client_verified = False
    bundle = None
    shim_executable = None
    host_identities = {}
    exit_queues = {}
    report = {"status": "failed", "phase": "prepare", "browser": str(browser),
              "output": str(output), "appId": app_id}
    log = (output / "browser.log").open("w")

    class Fixture(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            cookie = self.headers.get("Cookie", "")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            if self.path == "/login":
                self.send_header("Set-Cookie", f"floorp_cold={token}; Max-Age=600; HttpOnly; SameSite=Lax; Path=/")
            self.end_headers()
            authenticated = f"floorp_cold={token}" in cookie
            body = ("<!doctype html><meta charset=utf-8><title>Floorp cold launch fixture</title>"
                    "<style>body{background:#16324f;color:white;font:24px system-ui;padding:40px}</style>"
                    "<h1>Floorp cold launch fixture</h1><input id=editor><script>"
                    f"document.body.dataset.authenticated={json.dumps(str(authenticated).lower())};"
                    "document.body.dataset.sharedStorage=localStorage.getItem('floorp-cold')||'';"
                    "</script>")
            self.wfile.write(body.encode())

        def log_message(self, *_args):
            pass

    state = "Services.appShell.hiddenDOMWindow.wrappedJSObject.__floorpColdTest"
    service = "Cc['@floorp.org/mac-web-app-service;1'].getService(Ci.nsIMacWebAppService)"

    def connect():
        try:
            return smoke.Marionette(port)
        except (ConnectionRefusedError, socket.timeout):
            return None

    def attach_observer():
        client.context("chrome")
        client.script(f"""
            const old = {state};
            if (old?.observer) Services.obs.removeObserver(old.observer, 'floorp-web-app-shim-event');
            const current = {{appId: arguments[0], events: []}};
            current.observer = {{observe(_subject, _topic, data) {{
                const event = JSON.parse(data);
                if (event.appId === current.appId) current.events.push(event);
            }}}};
            {state} = current;
            Services.obs.addObserver(current.observer, 'floorp-web-app-shim-event');
        """, app_id)

    def native_handle():
        for handle in client.handles():
            try:
                client.switch(handle)
                client.context("chrome")
                if client.script(f"return !window.closed && window.gBrowserInit?.delayedStartupFinished && {service}.getAppIdForWindow(window) === arguments[0];", app_id):
                    return handle
            except RuntimeError as error:
                if "no such window" not in str(error).lower():
                    raise
        return None

    def verify_page(stage):
        handle = None

        def loaded_page():
            nonlocal handle
            handle = native_handle()
            if handle is None:
                return None
            try:
                client.switch(handle)
                client.context("content")
                return client.script("""
                    return document.querySelector('#editor') ? {
                        authenticated:document.body.dataset.authenticated,
                        sharedStorage:document.body.dataset.sharedStorage,
                        cookie:document.cookie
                    } : null;
                """)
            except RuntimeError as error:
                # PWA startup can replace its initial blank tab. Reacquire the
                # fully initialized app window instead of retaining that tab.
                if "no such window" not in str(error).lower():
                    raise
                return None

        page = smoke.wait_for(stage + " native page and session", loaded_page, args.timeout)
        if page["authenticated"] != "true" or page["sharedStorage"] != token:
            raise RuntimeError(f"{stage}: ordinary browser session was not shared: {page}")
        if "floorp_cold=" in page["cookie"]:
            raise RuntimeError("HttpOnly cookie exposed to page")
        client.context("chrome")
        if bundle is not None:
            chrome = smoke.wait_for(stage + " Floorp PWA chrome", lambda: client.script("""
                return window.wrappedJSObject.floorpSsbWindow === true ? {
                    initialized:true,
                    appId:document.documentElement.getAttribute('taskbartab'),
                    title:document.title
                } : null;
            """), args.timeout)
            if chrome["appId"] != app_id:
                raise RuntimeError(f"{stage}: PWA chrome belongs to a different app")
        before = client.script(f"return {state}.events.filter(event => event.type === 111).length;")
        client.context("content")
        client.script("document.body.style.backgroundColor = arguments[0];", "#" + uuid.uuid4().hex[:6])
        client.context("chrome")
        smoke.wait_for(stage + " authenticated compositor frame", lambda: client.script(
            f"return {state}.events.filter(event => event.type === 111).length > arguments[0];", before
        ), args.timeout)
        report[stage] = {"sharedSession": page, "nativeOwnership": True, "freshFrame": True}
        if bundle is not None:
            report[stage]["pwaChrome"] = chrome
            host_pid = client.script("return Services.appinfo.processID;")
            prefix = str(shim_executable) + " --host-service "
            peers = [row for row in smoke.HostProcess._process_rows()
                     if row[2].startswith(prefix) and f" --host-pid {host_pid} --launch-token " in row[2]]
            if len(peers) != 1:
                report[stage]["peerDiscovery"] = {
                    "hostPid": host_pid, "prefix": prefix,
                    "processes": [row for row in smoke.HostProcess._process_rows()
                                  if row[2].startswith(str(shim_executable) + " ")],
                    "connections": client.script(
                        f"return {state}.events.filter(event => event.type === 'connected');"),
                }
                raise RuntimeError(f"{stage}: expected exactly one native peer for this test host")
            shim_pid = peers[0][0]
            windows = smoke.wait_for(stage + " visible native window",
                                     lambda: smoke.native_window_ids(shim_pid), args.timeout)
            # Ask for a geometry report without requesting activation, to
            # preserve evidence of the Finder launch's foreground behavior.
            client.script("window.resizeBy(1, 0); window.resizeBy(-1, 0);")
            geometry = smoke.wait_for(stage + " native geometry", lambda: client.script(
                f"return {state}.events.filter(e => e.type === 101 && e.payload.kind === 'configured').at(-1)?.payload || null;"
            ), args.timeout)
            report[stage].update(shimPid=shim_pid, windowIds=windows, geometry=geometry)
            report[stage]["capture"] = smoke.capture_fixture_window(
                shim_pid, windows[0], output / (stage + "-window.png"))
        return handle

    def finder_open(bundle, label):
        # These environment variables enable only Marionette inspection. The
        # Shim's signed host/profile identifiers and authentication are intact.
        smoke.run("/usr/bin/open", "-n", "-a", str(bundle),
                  "--env", "MOZ_MARIONETTE=1", "--env", "MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1",
                  "--env", "MOZ_CRASHREPORTER_DISABLE=1",
                  *(argument for key, value in profile_environment.items()
                    for argument in ("--env", f"{key}={value}")),
                  "--stdout", str(output / (label + ".stdout.log")),
                  "--stderr", str(output / (label + ".stderr.log")))

    def quit_host():
        nonlocal client, client_verified
        if not client_verified:
            raise RuntimeError("Refusing browser cleanup before process/profile verification")
        client.context("chrome")
        client.script(f"""
            const current = {state};
            if (current?.observer) Services.obs.removeObserver(current.observer, 'floorp-web-app-shim-event');
            const {{setTimeout}} = ChromeUtils.importESModule('resource://gre/modules/Timer.sys.mjs');
            setTimeout(() => Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit), 0);
        """)
        client.close()
        client = None
        client_verified = False

    def alive(pid):
        if host is not None and pid == host.pid:
            return host.poll() is None
        identity = host_identities.get(pid)
        return identity is not None and identity in smoke.HostProcess._process_rows(pid)

    def remember_host(pid, command):
        identity = next((row for row in smoke.HostProcess._process_rows(pid)
                         if row[0] == pid and row[2] == command), None)
        if identity is None:
            raise RuntimeError("Test host executable/profile command does not match its PID")
        host_identities[pid] = identity
        if host is None or pid != host.pid:
            queue = select.kqueue()
            queue.control([select.kevent(pid, filter=select.KQ_FILTER_PROC,
                flags=select.KQ_EV_ADD | select.KQ_EV_ONESHOT,
                fflags=select.KQ_NOTE_EXIT | 0x04000000)], 0, 0)
            exit_queues[pid] = queue
            if identity not in smoke.HostProcess._process_rows(pid):
                raise RuntimeError("Test host changed during process monitoring")
        report.setdefault("hostIdentities", []).append(
            {"pid": pid, "startTime": identity[1], "command": identity[2]})

    def verify_clean_exit(pid):
        if host is not None and pid == host.pid:
            status = host.wait(timeout=5)
        else:
            events = exit_queues[pid].control(None, 1, 2)
            status = next((os.waitstatus_to_exitcode(event.data & 0xffff)
                           for event in events
                           if event.ident == pid and event.fflags & 0x04000000), None)
        report.setdefault("hostExitCodes", {})[str(pid)] = status
        if status != 0:
            raise RuntimeError(f"Test browser failed to exit cleanly: {status}")

    try:
        smoke.run("/usr/bin/codesign", "--verify", "--strict", str(browser))
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        base = f"http://127.0.0.1:{server.server_port}"
        manifest = {"id": app_id, "name": "Floorp Cold " + uuid.uuid4().hex[:8],
                    "start_url": base + "/app", "scope": base + "/", "icon": "", "userContextId": 0}
        (profile / "ssb").mkdir(mode=0o700)
        # Publish the SSB only after its scoped native installation commits.
        # Otherwise normal startup repair could install a legacy launcher in
        # the real user's Applications directory before this fixture takes over.
        (profile / "ssb/ssb.json").write_text("{}\n")
        port = smoke.unused_port()
        prefs = {"marionette.port": port, "browser.shell.checkDefaultBrowser": False,
                 "browser.startup.homepage_override.mstone": "ignore", "browser.startup.page": 0,
                 "browser.aboutwelcome.enabled": False, "browser.warnOnQuit": False,
                 "browser.tabs.warnOnClose": False, "browser.sessionstore.resume_from_crash": False,
                 "datareporting.policy.dataSubmissionEnabled": False,
                 "floorp.browser.nativeApp.appShim.enabled": True,
                 "floorp.browser.nativeApp.keepRunningAfterBrowserQuit": False}
        (profile / "user.js").write_text("\n".join(
            f"user_pref({json.dumps(key)}, {json.dumps(value)});" for key, value in prefs.items()
        ) + "\n")
        env = {key: value for key, value in os.environ.items()
               if key not in ("MOZ_NO_REMOTE", "MOZ_NEW_INSTANCE") and key not in profile_environment}
        env.update(MOZ_CRASHREPORTER_DISABLE="1", MOZ_MARIONETTE="1", MOZ_REMOTE_ALLOW_SYSTEM_ACCESS="1")
        report["phase"] = "initial-host"
        # Deliberately allow remoting: the warm Finder launch must reach this
        # existing browser parent and profile instead of opening a second host.
        host_command = [str(binary), "--profile", str(profile), "--marionette",
                        "--remote-allow-system-access", "about:blank"]
        host = subprocess.Popen(host_command,
                                stdout=log, stderr=subprocess.STDOUT, env=env)
        remember_host(host.pid, " ".join(host_command))
        client = smoke.wait_for("initial Marionette", connect, args.timeout)
        capabilities = client.session.get("capabilities", {})
        if capabilities.get("moz:processID") != host.pid or capabilities.get("moz:profile") != str(profile):
            raise RuntimeError("Marionette connected to an unexpected initial browser or profile")
        client_verified = True
        normal_handle = client.handles()[0]
        client.context("chrome")
        registered = smoke.wait_for("normal Floorp command-line registration", lambda: client.script("""
            try {
                return Services.catMan.getCategoryEntry('command-line-handler', 'm-floorp-ssb');
            } catch { return null; }
        """), args.timeout)
        if registered != "@noraneko.org/commandlinehandler/general-start-ssb;1":
            raise RuntimeError("Normal Floorp startup did not register --start-ssb")
        report["registeredHandler"] = registered
        capabilities = json.loads(client.script(f"return {service}.capabilitiesJSON;"))
        if not all(capabilities.get(key) is True for key in (
            "nativeWindowOwnership", "authenticatedTransport", "sharedBrowserProfile", "transactionalInstall"
        )):
            raise RuntimeError(f"Native service capability unavailable: {capabilities}")
        report["capabilities"] = capabilities
        attach_observer()
        client.context("content")
        client.navigate(base + "/login")
        client.script("localStorage.setItem('floorp-cold', arguments[0]);", token)
        client.context("chrome")
        client.script(f"""
            const {{MacNativeAppRuntime}} = ChromeUtils.importESModule('resource://noraneko/modules/pwa/NativeAppRuntime.sys.mjs');
            const {{BrowserWindowTracker}} = ChromeUtils.importESModule('resource:///modules/BrowserWindowTracker.sys.mjs');
            const current = {state};
            current.runtime = new MacNativeAppRuntime({{
                applicationsDirectory: arguments[0], profileDirectory: PathUtils.profileDir,
                browserExecutable: Services.dirsvc.get('XREExeF', Ci.nsIFile).path,
                shimExecutable: PathUtils.join(Services.dirsvc.get('GreBinD', Ci.nsIFile).path, 'floorp-app-shim')
            }});
            current.launchDone = false;
            current.runtime.open(arguments[1], () => {{
                const url = Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);
                url.data = arguments[1].start_url;
                return BrowserWindowTracker.openWindow({{args: url, features:'width=900,height=700'}});
            }}).then(win => {{current.launchDone = true; current.launchError = win ? null : 'Native launch unavailable';}},
                     error => {{current.launchDone = true; current.launchError = String(error);}});
        """, str(output / "Applications"), manifest)
        smoke.wait_for("initial install", lambda: client.script(f"return {state}.launchDone;"), args.timeout)
        error = client.script(f"return {state}.launchError;")
        if error:
            raise RuntimeError(error)
        verify_page("initialInstall")
        registry = json.loads((profile / "ssb/app-registry.json").read_text())
        installed = next(app for app in registry["apps"] if app["installId"] == app_id)
        bundle = Path(installed["bundlePath"])
        if output not in bundle.parents or installed["integration"] != "app-shim" or not installed.get("installedShim"):
            raise RuntimeError("Native install did not commit inside the disposable fixture")
        with (bundle / "Contents/Info.plist").open("rb") as stream:
            executable_name = plistlib.load(stream)["CFBundleExecutable"]
        if Path(executable_name).name != executable_name:
            raise RuntimeError("Installed Shim has an invalid executable name")
        shim_executable = bundle / "Contents/MacOS" / executable_name
        report["bundle"] = str(bundle)
        client.context("chrome")
        client.script(f"""
            const current = {state};
            const {{DataStoreProvider}} = ChromeUtils.importESModule('resource://noraneko/modules/pwa/DataStore.sys.mjs');
            current.saved = false;
            DataStoreProvider.getDataManager().saveSsbData(arguments[0]).then(
                () => {{current.saved = true;}},
                error => {{current.launchError = String(error); current.saved = true;}}
            );
        """, manifest)
        smoke.wait_for("publish committed SSB", lambda: client.script(f"return {state}.saved;"), args.timeout)
        error = client.script(f"return {state}.launchError;")
        if error:
            raise RuntimeError(error)
        client.script(f"""
            const current = {state};
            current.closeDone = false;
            current.runtime.stopForMutation(arguments[0]).then(closed => {{
                if (!closed) throw new Error('Fixture unexpectedly vetoed close');
                current.runtime.dispose(); current.closeDone = true;
            }}).catch(error => {{current.launchError = String(error); current.closeDone = true;}});
        """, app_id)
        client.switch(normal_handle)
        smoke.wait_for("close initial app", lambda: client.script(f"return {state}.closeDone;"), args.timeout)
        error = client.script(f"return {state}.launchError;")
        if error:
            raise RuntimeError(error)
        report["phase"] = "warm-finder"
        attach_observer()
        finder_open(bundle, "warm-finder")
        verify_page("warmFinder")
        pid = client.script("return Services.appinfo.processID;")
        if pid != host.pid:
            raise RuntimeError("Warm Finder launch changed browser parent")
        report["warmFinder"]["sameHostPid"] = pid
        quit_host()
        smoke.wait_for("original host exit", lambda: not alive(host.pid), args.timeout)
        verify_clean_exit(host.pid)
        report["phase"] = "cold-finder"
        finder_open(bundle, "cold-finder")
        client = smoke.wait_for("cold Marionette", connect, args.timeout)
        capabilities = client.session.get("capabilities", {})
        cold_pid = capabilities.get("moz:processID")
        if type(cold_pid) is not int or cold_pid <= 0 or cold_pid == host.pid or capabilities.get("moz:profile") != str(profile):
            raise RuntimeError("Cold launch did not reopen the same profile in a new browser parent")
        remember_host(cold_pid, f"{binary} --profile {profile} --start-ssb {app_id}")
        client_verified = True
        client.context("chrome")
        attach_observer()
        verify_page("coldFinder")
        report["coldFinder"]["newHostPid"] = cold_pid
        if args.inspect_seconds:
            print(json.dumps({"inspectBundle": str(bundle), "output": str(output),
                              "seconds": args.inspect_seconds}), flush=True)
            time.sleep(args.inspect_seconds)
        quit_host()
        smoke.wait_for("cold host exit", lambda: not alive(cold_pid), args.timeout)
        verify_clean_exit(cold_pid)
        if any(unrelated_profile.iterdir()) or any(unrelated_local.iterdir()):
            raise RuntimeError("Inherited environment caused writes to an unrelated profile")
        report["profileEnvironmentIsolation"] = {
            "ignoredVariables": list(profile_environment), "unrelatedProfileUntouched": True,
        }
        report["status"] = "passed"
        report["phase"] = "complete"
    except Exception as error:
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc(limit=5)
    finally:
        if client:
            try:
                quit_host()
            except Exception:
                if client_verified:
                    client.close()
                else:
                    client.socket.close()
        # If cold startup failed before Marionette, locate only the exact
        # executable/profile/app-id tuple this fixture asked the Shim to spawn.
        # Never match a browser name or an unrelated user's profile.
        cold_prefix = f"{binary} --profile {profile} --start-ssb {app_id}"
        fixture_shims = []
        for identity in smoke.HostProcess._process_rows():
            pid, _, command = identity
            if command == cold_prefix:
                host_identities.setdefault(pid, identity)
            if shim_executable is not None and command.startswith(
                str(shim_executable) + " --host-service "
            ):
                fixture_shims.append(identity)
        # Kill only PIDs returned by this fixture's browser/Marionette session
        # or its exact cold-launch command above.
        # Keep the disposable profile, receipt and logs for failure diagnosis.
        for pid, identity in host_identities.items():
            if identity in smoke.HostProcess._process_rows(pid):
                try:
                    os.kill(pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
        if host:
            try:
                host.wait(timeout=10)
            except subprocess.TimeoutExpired:
                host.kill()
                host.wait(timeout=10)
        for identity in fixture_shims:
            pid = identity[0]
            if identity in smoke.HostProcess._process_rows(pid):
                try:
                    os.kill(pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
        # A reparented cold host has no Popen.wait(), and a Shim can outlive its
        # host briefly. Do not hand off the GUI while an owned process remains.
        for identity in [*host_identities.values(), *fixture_shims]:
            pid = identity[0]
            try:
                for requested_signal in (None, signal.SIGKILL):
                    if identity not in smoke.HostProcess._process_rows(pid):
                        break
                    if requested_signal is not None:
                        # Recheck the full generation immediately before escalation.
                        if identity not in smoke.HostProcess._process_rows(pid):
                            break
                        os.kill(pid, requested_signal)
                    deadline = time.monotonic() + 5
                    while identity in smoke.HostProcess._process_rows(pid):
                        if time.monotonic() >= deadline:
                            break
                        time.sleep(0.1)
                if identity in smoke.HostProcess._process_rows(pid):
                    raise RuntimeError(f"Owned test process did not exit: {pid}")
            except ProcessLookupError:
                pass
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                report.setdefault("cleanupErrors", []).append(str(error))
                report["status"] = "failed"
        for queue in exit_queues.values():
            queue.close()
        if server:
            server.shutdown()
            server.server_close()
        log.close()
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
