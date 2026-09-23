#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run the colocated PWA module tests in a built browser, in a disposable profile."""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import signal
import socket
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path)
    parser.add_argument("--modules", type=Path, default=Path("bridge/loader-modules/_dist"))
    parser.add_argument("--output", type=Path, default=Path("_dist/app-shim-browser-tests"))
    parser.add_argument("--module", action="append", choices=(
        "modules/pwa/test/appLifecycle.test.mjs",
        "modules/pwa/test/MacAppShimInstaller.test.mjs",
        "modules/pwa/test/NativeAppRuntime.test.mjs",
        "modules/pwa/supports/test/MacOS.test.mjs",
    ), help="Run only this colocated suite (repeat to select several)")
    args = parser.parse_args()
    browser = args.browser.resolve(strict=True)
    modules = args.modules.resolve(strict=True)
    spec = importlib.util.spec_from_file_location("runtime_smoke", Path(__file__).with_name("test-runtime.py"))
    smoke = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(smoke)
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    profile = output / "profile"
    profile.mkdir(mode=0o700)
    with (browser / "Contents/Info.plist").open("rb") as stream:
        binary = browser / "Contents/MacOS" / plistlib.load(stream)["CFBundleExecutable"]
    port = smoke.unused_port()
    prefs = {
        "marionette.port": port,
        "browser.shell.checkDefaultBrowser": False,
        "browser.startup.homepage_override.mstone": "ignore",
        "browser.startup.page": 0,
        "browser.aboutwelcome.enabled": False,
        "datareporting.policy.dataSubmissionEnabled": False,
        "browser.warnOnQuit": False,
    }
    (profile / "user.js").write_text("\n".join(f"user_pref({json.dumps(key)}, {json.dumps(value)});" for key, value in prefs.items()) + "\n")
    report = {"status": "failed", "modules": [], "profile": str(profile)}
    host = client = None
    with (output / "browser.log").open("w") as log:
        try:
            # Classic Marionette automatically accepts every beforeunload
            # prompt. A real BiDi session lets the lifecycle fixture exercise
            # its intended user veto instead of silently losing the page.
            bidi = not args.module or "modules/pwa/test/NativeAppRuntime.test.mjs" in args.module
            command = [str(binary), "--no-remote", "--profile", str(profile), "--marionette", "--remote-allow-system-access"]
            if bidi:
                command.append("--remote-debugging-port=0")
            command.append("about:blank")
            host = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env={**os.environ, "MOZ_CRASHREPORTER_DISABLE": "1"})

            def connect():
                if host.poll() is not None:
                    raise RuntimeError(f"Browser exited {host.returncode}")
                try:
                    return smoke.Marionette(port, bidi=bidi)
                except (ConnectionRefusedError, socket.timeout):
                    return None

            client = smoke.wait_for("Marionette", connect, 90)
            client.context("chrome")
            client.script("""
                const directory = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
                directory.initWithPath(arguments[0]);
                Services.io.getProtocolHandler('resource').QueryInterface(Ci.nsIResProtocolHandler)
                    .setSubstitution('noraneko', Services.io.newFileURI(directory));
                return true;
            """, str(modules))
            for name in args.module or (
                "modules/pwa/test/appLifecycle.test.mjs",
                "modules/pwa/test/MacAppShimInstaller.test.mjs",
                "modules/pwa/test/NativeAppRuntime.test.mjs",
                "modules/pwa/supports/test/MacOS.test.mjs",
            ):
                client.script("""
                    const state = window.wrappedJSObject;
                    state.__appShimModuleTest = {done:false};
                    try {
                        const module = ChromeUtils.importESModule('resource://noraneko/' + arguments[0]);
                        Promise.resolve(module.runAllTests()).then(
                            () => { state.__appShimModuleTest = {done:true, passed:true}; },
                            error => { state.__appShimModuleTest = {done:true, passed:false, error:String(error), stack:error.stack}; }
                        );
                    } catch (error) {
                        state.__appShimModuleTest = {done:true, passed:false, error:String(error), stack:error.stack};
                    }
                """, name)
                result = smoke.wait_for(name, lambda: client.script("return window.wrappedJSObject.__appShimModuleTest.done ? window.wrappedJSObject.__appShimModuleTest : null;"), 180)
                report["modules"].append({"name": name, **result})
                print(f"{'PASS' if result.get('passed') else 'FAIL'} {name}", flush=True)
            report["status"] = "passed" if all(module.get("passed") for module in report["modules"]) else "failed"
        except Exception as error:
            report["error"] = str(error)
        finally:
            if client:
                client.close()
            requested_termination = False
            if host and host.poll() is None:
                requested_termination = True
                host.terminate()
                try:
                    host.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    host.kill()
                    host.wait(timeout=5)
            if host:
                code = host.poll()
                report["hostExitCode"] = code
                report["cleanupRequestedSIGTERM"] = requested_termination
                if code != 0 and not (requested_termination and code == -signal.SIGTERM):
                    report["status"] = "failed"
                    report["shutdownError"] = f"Browser exited unexpectedly: {code}"
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"{report['status']}: {output / 'report.json'}")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
