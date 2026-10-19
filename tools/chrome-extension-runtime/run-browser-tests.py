#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Bundle and run Chrome extension compatibility suites in an isolated browser."""

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import signal
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
from urllib.parse import parse_qs, urlencode, urlsplit


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]


class FixtureHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path.startswith("/debugger-echo"):
            self._debugger_echo()
        elif self.path.startswith("/identity-"):
            self._identity()
        else:
            super().do_GET()

    def do_POST(self):
        if self.path.startswith("/debugger-echo"):
            self._debugger_echo()
        else:
            self.send_error(404)

    def _debugger_echo(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        payload = json.dumps({"method": self.command, "headers": dict(self.headers),
                              "body": body.decode("utf-8")}).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("X-Debugger-Fixture", "native-network-response")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def _identity(self):
        """Local OAuth transport fixtures; never contact an external provider."""
        request = urlsplit(self.path)
        query = parse_qs(request.query)
        target = query.get("redirect_uri", [""])[0]
        if request.path == "/identity-token":
            fragment = {
                "access_token": "loopback-test-token",
                "token_type": "Bearer", "expires_in": "3600",
                "state": query.get("state", [""])[0],
                "scope": query.get("scope", [""])[0],
            }
            mode = query.get("fixture_mode", [""])[0]
            if mode == "bad-state":
                fragment["state"] = "unexpected-state"
            elif mode == "missing-scope":
                fragment["scope"] = "different.scope"
            elif mode == "denied":
                fragment = {"state": fragment["state"], "error": "access_denied"}
            target += "#" + urlencode(fragment)
        if request.path in ("/identity-redirect", "/identity-token"):
            # The browser intercepts this exact extension redirect before loading it.
            self.send_response(302)
            self.send_header("Location", target)
            payload = b""
        elif request.path == "/identity-interactive":
            self.send_response(200)
            payload = ("<!doctype html><title>OAuth loopback fixture</title>"
                       "<script>setTimeout(() => location.href = " +
                       json.dumps(target) + ", 200)</script>").encode("utf-8")
        elif request.path in ("/identity-idle", "/identity-no-redirect"):
            self.send_response(200)
            payload = b"<!doctype html><title>OAuth loopback awaiting consent</title>"
        else:
            self.send_error(404)
            return
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)


def browser_binary(path):
    if path.is_file():
        return path
    if (path / "Contents/Info.plist").is_file():
        with (path / "Contents/Info.plist").open("rb") as stream:
            return path / "Contents/MacOS" / plistlib.load(stream)["CFBundleExecutable"]
    for name in ("floorp", "floorp.exe", "firefox", "firefox.exe"):
        if (path / name).is_file():
            return path / name
    raise ValueError(f"No browser executable found in {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path)
    parser.add_argument("--test", action="append", type=Path,
                        help="Browser suite exporting runAllTests (repeatable)")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "_dist/chrome-runtime-tests")
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--actor-module", action="append", type=Path, default=[],
                        help="Private test actor module copied into the profile before launch (repeatable)")
    args = parser.parse_args()
    binary = browser_binary(args.browser.resolve(strict=True))
    tests = args.test or [ROOT / "browser-features/modules/modules/chrome-web-store/test/polyfills/documentId/DocumentIdPolyfill.test.mts"]
    tests = [path.resolve(strict=True) for path in tests]
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    bundles = output / "modules"
    bundles.mkdir()
    browser_globals = bundles / "browser-globals.mjs"
    browser_globals.write_text("""
        export const window = Services.wm.getMostRecentWindow('navigator:browser');
        export const document = window.document;
        export const setTimeout = window.setTimeout.bind(window);
        export const clearTimeout = window.clearTimeout.bind(window);
        export const setInterval = window.setInterval.bind(window);
        export const clearInterval = window.clearInterval.bind(window);
    """)
    # Deno's Windows .cmd shim is a shell script, not an executable accepted by
    # CreateProcess. Invoke the real package's JavaScript entry through Node on
    # every platform and keep arguments out of a command shell.
    esbuild_entries = sorted((ROOT / "node_modules/.deno").glob(
        "esbuild@*/node_modules/esbuild/bin/esbuild"))
    esbuild_entries.append(ROOT / "node_modules/esbuild/bin/esbuild")
    esbuild = next((path for path in esbuild_entries if path.is_file()), None)
    if not esbuild:
        parser.error("Install project dependencies before running browser suites (esbuild is required)")
    for index, test in enumerate(tests):
        subprocess.run(["node", str(esbuild), str(test), "--bundle", "--format=esm",
                        f"--inject:{browser_globals}",
                        f"--outfile={bundles / f'test-{index}.mjs'}"],
                       check=True, cwd=ROOT)

    spec = importlib.util.spec_from_file_location(
        "runtime_smoke", ROOT / "tools/app-shim/test-runtime.py")
    smoke = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(smoke)
    profile = output / "profile"
    profile.mkdir(mode=0o700)
    actor_directory = profile / "chrome/runtime-actors"
    actor_names = set()
    for module in args.actor_module:
        module = module.resolve(strict=True)
        if not module.is_file() or module.name in actor_names:
            parser.error("Actor modules must be distinct files with unique names")
        actor_names.add(module.name)
        actor_directory.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(module, actor_directory / module.name)
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
    (profile / "user.js").write_text("\n".join(
        f"user_pref({json.dumps(key)}, {json.dumps(value)});"
        for key, value in prefs.items()) + "\n")
    report = {"passed": False, "browser": str(binary), "modules": []}
    client = host = None
    fixture_handler = partial(FixtureHandler, directory=str(
        ROOT / "tools/chrome-extension-runtime/fixtures"))
    fixture_server = ThreadingHTTPServer(("127.0.0.1", 0), fixture_handler)
    fixture_thread = threading.Thread(target=fixture_server.serve_forever, daemon=True)
    fixture_thread.start()
    fixture_url = f"http://127.0.0.1:{fixture_server.server_port}/"
    with (output / "browser.log").open("w") as log:
        try:
            command = [str(binary), "--no-remote", "--profile", str(profile),
                       "--marionette", "--remote-allow-system-access"]
            if not args.headed:
                command.append("--headless")
            command.append("about:blank")
            host = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                    env={**os.environ, "MOZ_CRASHREPORTER_DISABLE": "1",
                                         "FLOORP_TEST_ACTOR_DIRECTORY": str(actor_directory) if args.actor_module else ""})

            def connect():
                if host.poll() is not None:
                    raise RuntimeError(f"Browser exited {host.returncode}")
                try:
                    return smoke.Marionette(port)
                except (ConnectionRefusedError, socket.timeout):
                    return None

            client = smoke.wait_for("Marionette", connect, 45)
            client.context("chrome")
            client.script("""
                window.wrappedJSObject.__floorpRuntimeFixtureURL = arguments[0];
            """, fixture_url)
            report["runtime"] = client.script("""
                const {AppConstants} = ChromeUtils.importESModule(
                    'resource://gre/modules/AppConstants.sys.mjs');
                return {
                    version: Services.appinfo.version,
                    buildId: Services.appinfo.appBuildID,
                    os: Services.appinfo.OS,
                    abi: Services.appinfo.XPCOMABI,
                    workerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
                    workerWebIDLCompiled: AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
                };
            """)
            client.script("""
                const directory = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
                directory.initWithPath(arguments[0]);
                Services.io.getProtocolHandler('resource').QueryInterface(Ci.nsIResProtocolHandler)
                    .setSubstitution('floorp-runtime-tests', Services.io.newFileURI(directory));
            """, str(bundles))
            for index, test in enumerate(tests):
                client.script("""
                    const state = window.wrappedJSObject;
                    state.__floorpRuntimeTest = {done: false};
                    try {
                        const suite = ChromeUtils.importESModule(
                            'resource://floorp-runtime-tests/' + arguments[0]);
                        Promise.resolve(suite.runAllTests()).then(
                            () => { state.__floorpRuntimeTest = {done: true, passed: true}; },
                            error => { state.__floorpRuntimeTest = {
                                done: true, passed: false, error: String(error), stack: error.stack,
                            }; }
                        );
                    } catch (error) {
                        state.__floorpRuntimeTest = {
                            done: true, passed: false, error: String(error), stack: error.stack,
                        };
                    }
                """, f"test-{index}.mjs")
                result = smoke.wait_for(str(test), lambda: client.script("""
                    return window.wrappedJSObject.__floorpRuntimeTest.done
                        ? window.wrappedJSObject.__floorpRuntimeTest : null;
                """), args.timeout)
                report["modules"].append({"path": str(test), **result})
                print(f"{'PASS' if result.get('passed') else 'FAIL'} {test}", flush=True)
            report["passed"] = all(result["passed"] for result in report["modules"])
        except Exception as error:
            report["error"] = str(error)
        finally:
            fixture_server.shutdown()
            fixture_server.server_close()
            fixture_thread.join(timeout=5)
            if client:
                client.close()
            if host:
                requested_termination = host.poll() is None
                if requested_termination:
                    host.terminate()
                    try:
                        host.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        host.kill()
                        host.wait(timeout=5)
                report["hostExitCode"] = host.returncode
                if host.returncode != 0 and not (
                    requested_termination and (host.returncode == -signal.SIGTERM or os.name == "nt")
                ):
                    report["passed"] = False
                    report["shutdownError"] = f"Unexpected browser exit: {host.returncode}"
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{'passed' if report['passed'] else 'failed'}: {output / 'report.json'}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
