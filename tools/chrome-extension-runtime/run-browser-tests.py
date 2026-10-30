#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Bundle and run Chrome extension compatibility suites in an isolated browser."""

import argparse
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import signal
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import uuid
from urllib.parse import parse_qs, urlencode, urlsplit


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]


class UserScriptsParserBarriers:
    def __init__(self):
        self.lock = threading.Lock()
        self.states = {}

    def create(self, token):
        with self.lock:
            if token in self.states or len(self.states) >= 64:
                raise ValueError("Parser barrier token is reused or capacity exceeded")
            self.states[token] = {"event": threading.Event(), "scriptRequested": False,
                                  "released": False, "timedOut": False}

    def wait(self, token):
        with self.lock:
            state = self.states[token]
            state["scriptRequested"] = True
        finished = state["event"].wait(30)
        with self.lock:
            state["timedOut"] = not finished

    def status(self, token):
        with self.lock:
            state = self.states.get(token, {})
            return {key: state.get(key, False) for key in
                    ("scriptRequested", "released", "timedOut")}

    def release(self, token):
        with self.lock:
            state = self.states[token]
            state["released"] = True
            state["event"].set()

    def close(self):
        with self.lock:
            for state in self.states.values():
                state["event"].set()


class FixtureHandler(SimpleHTTPRequestHandler):
    # The exact authored programs are data for browser realms. Their .txt
    # witnesses stay outside host program discovery, while URLs stay unchanged.
    AUTHORED_SCRIPT_PATHS = frozenset(
        f"/{realm}/{name}.js"
        for realm in ("content-worlds", "custom-event-detail")
        for name in ("observe-detail", "probe", "publish")
    )

    def translate_path(self, path):
        translated = super().translate_path(path)
        if urlsplit(path).path in self.AUTHORED_SCRIPT_PATHS:
            return translated + ".txt"
        return translated

    def guess_type(self, path):
        if str(path).endswith(".js.txt"):
            return "text/javascript"
        return super().guess_type(path)

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path.startswith("/user-scripts-consent/"):
            self._user_scripts_consent()
        elif self.path.startswith("/debugger-echo"):
            self._debugger_echo()
        elif self.path.startswith("/identity-"):
            self._identity()
        else:
            super().do_GET()

    def do_POST(self):
        if self.path.startswith("/user-scripts-consent/"):
            self._user_scripts_consent()
        elif self.path.startswith("/debugger-echo"):
            self._debugger_echo()
        else:
            self.send_error(404)

    def _user_scripts_consent(self):
        request = urlsplit(self.path)
        values = parse_qs(request.query).get("token", [])
        if len(values) != 1 or re.fullmatch(r"[0-9a-f]{32}", values[0]) is None:
            self.send_error(400, "Expected an owned parser barrier token")
            return
        token = values[0]
        barriers = self.server.user_scripts_barriers
        content_type = "application/json"
        try:
            if request.path == "/user-scripts-consent/held.html" and self.command == "GET":
                barriers.create(token)
                body = ('<!doctype html><title>Floorp held user scripts document</title>'
                        '<script src="/user-scripts-consent/barrier.js?token=' + token + '"></script>'
                        '<main id="user-script-consent-fixture">Owned delayed parser document</main>').encode()
                content_type = "text/html; charset=utf-8"
            elif request.path == "/user-scripts-consent/barrier.js" and self.command == "GET":
                barriers.wait(token)
                body = b"// Owned parser barrier completed.\n"
                content_type = "text/javascript; charset=utf-8"
            elif request.path == "/user-scripts-consent/status" and self.command == "GET":
                body = json.dumps(barriers.status(token)).encode()
            elif request.path == "/user-scripts-consent/release" and self.command == "POST":
                barriers.release(token)
                body = json.dumps(barriers.status(token)).encode()
            else:
                self.send_error(405, "Unsupported parser barrier operation")
                return
        except KeyError:
            self.send_error(404, "Unknown parser barrier token")
            return
        except ValueError as error:
            self.send_error(409, str(error))
            return
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

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


def file_record(path):
    path = Path(path).resolve(strict=True)
    if not path.is_file():
        raise ValueError(f"Expected a source file: {path}")
    data = path.read_bytes()
    return {"path": str(path), "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}


def verify_file_records(records):
    for record in records:
        current = file_record(record["path"])
        if any(current[key] != record[key] for key in ("path", "sha256", "bytes")):
            raise RuntimeError(f"Execution source changed: {record['path']}")


def bundle_input_records(metafile):
    inputs = json.loads(Path(metafile).read_text(encoding="utf-8"))["inputs"]
    if not isinstance(inputs, dict) or not inputs:
        raise ValueError("Bundled tests must identify all esbuild input files")
    records = []
    for name, info in inputs.items():
        record = file_record(ROOT / name)
        if type(info.get("bytes")) is not int or info["bytes"] != record["bytes"]:
            raise RuntimeError(f"Bundled input size differs from the source: {name}")
        records.append(record)
    if len({record["path"] for record in records}) != len(records):
        raise ValueError("Bundled inputs contain duplicate canonical source paths")
    return sorted(records, key=lambda record: record["path"])


def esbuild_command(esbuild):
    # esbuild's install script may replace its Node entry with the native
    # executable. Launch ELF, PE and Mach-O directly; keep JavaScript wrappers
    # under Node, including Windows where .cmd shims require a command shell.
    with Path(esbuild).open("rb") as stream:
        header = stream.read(4)
    native_headers = (
        b"\x7fELF", b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe",
        b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
        b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
        b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca",
    )
    if header in native_headers or header.startswith(b"MZ"):
        return [str(esbuild)]
    return ["node", str(esbuild)]


def bundle_test(esbuild, test, browser_globals, bundle, metafile):
    source = file_record(test)
    command = [*esbuild_command(esbuild), str(test), "--bundle", "--format=esm",
               f"--inject:{browser_globals}", f"--outfile={bundle}", f"--metafile={metafile}"]
    # Discover dependencies first, then bind the actual executed build to their
    # before/after bytes. Entry-only hashes miss changes to imported operations.
    subprocess.run([*command, "--log-level=error"], check=True, cwd=ROOT)
    inputs = bundle_input_records(metafile)
    subprocess.run(command, check=True, cwd=ROOT)
    if bundle_input_records(metafile) != inputs:
        raise RuntimeError(f"Test dependency graph or source changed while bundling: {test}")
    verify_file_records([source, *inputs])
    return {
        "testSourceSha256": source["sha256"],
        "bundleSha256": file_record(bundle)["sha256"],
        "bundleMetafileSha256": file_record(metafile)["sha256"],
        "bundleInputs": inputs,
    }


def copy_actor_module(source, actor_directory, retained_directory):
    record = file_record(source)
    for directory in (actor_directory, retained_directory):
        directory.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(record["path"], directory / Path(record["path"]).name)
    record.update({
        "loadedPath": str((actor_directory / Path(record["path"]).name).resolve()),
        "copiedPath": str((retained_directory / Path(record["path"]).name).resolve()),
    })
    verify_actor_modules([record])
    return record


def verify_actor_modules(records):
    for record in records:
        verify_file_records([record, *[
            {**record, "path": record[key]} for key in ("loadedPath", "copiedPath")
        ]])


def verify_execution_sources(provenance, bundles, actors):
    for index, record in enumerate(provenance):
        verify_file_records(record["bundleInputs"])
        metafile = bundles / f"test-{index}.meta.json"
        if (file_record(metafile)["sha256"] != record["bundleMetafileSha256"] or
                bundle_input_records(metafile) != record["bundleInputs"] or
                file_record(bundles / f"test-{index}.mjs")["sha256"] != record["bundleSha256"]):
            raise RuntimeError("Executed bundle or complete input manifest changed")
    verify_actor_modules(actors)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path)
    parser.add_argument("--test", action="append", type=Path,
                        help="Browser suite exporting runAllTests (repeatable)")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "_dist/chrome-runtime-tests")
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--candidate-manifest", type=Path,
                        default=Path(os.environ["FLOORP_TEST_CANDIDATE_MANIFEST"])
                        if os.environ.get("FLOORP_TEST_CANDIDATE_MANIFEST") else None,
                        help="Require this exact production native package, source and build before running suites")
    parser.add_argument("--environment-proxy", action="store_true",
                        help="Opt in to normal environment HTTP(S)_PROXY/NO_PROXY and public CA trust in this disposable profile only")
    parser.add_argument("--actor-module", action="append", type=Path, default=[],
                        help="Private test actor module copied into the profile before launch (repeatable)")
    args = parser.parse_args()
    binary = browser_binary(args.browser.resolve(strict=True))
    candidate = candidate_identity = None
    if args.candidate_manifest:
        candidate_spec = importlib.util.spec_from_file_location(
            "candidate_identity", ROOT / "tools/chrome-extension-runtime/candidate-identity.py")
        candidate_identity = importlib.util.module_from_spec(candidate_spec)
        candidate_spec.loader.exec_module(candidate_identity)
        candidate = candidate_identity.authenticate_candidate(
            args.candidate_manifest.resolve(strict=True), binary, ROOT)
    candidate_provenance = {}
    if candidate is not None:
        manifest = candidate["manifest"]
        candidate_provenance = {
            "manifestSha256": hashlib.sha256(args.candidate_manifest.read_bytes()).hexdigest(),
            "packageSha256": candidate["packageSha256"],
            **{key: manifest[key] for key in
               ("sourceCommit", "floorpCommit", "patches", "buildId", "platform")},
        }
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
    """, encoding="utf-8", newline="\n")
    # Use the package entry instead of a shell-dependent .cmd shim. Its bytes
    # determine whether installation left a Node wrapper or a native executable.
    esbuild_entries = sorted((ROOT / "node_modules/.deno").glob(
        "esbuild@*/node_modules/esbuild/bin/esbuild"))
    esbuild_entries.append(ROOT / "node_modules/esbuild/bin/esbuild")
    esbuild = next((path for path in esbuild_entries if path.is_file()), None)
    if not esbuild:
        parser.error("Install project dependencies before running browser suites (esbuild is required)")
    test_provenance = []
    for index, test in enumerate(tests):
        test_provenance.append(bundle_test(
            esbuild, test, browser_globals, bundles / f"test-{index}.mjs",
            bundles / f"test-{index}.meta.json"))

    spec = importlib.util.spec_from_file_location(
        "runtime_smoke", ROOT / "tools/app-shim/test-runtime.py")
    smoke = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(smoke)
    profile = output / "profile"
    profile.mkdir(mode=0o700)
    actor_directory = profile / "chrome/runtime-actors"
    actor_names = set()
    actor_modules = []
    for module in args.actor_module:
        module = module.resolve(strict=True)
        if not module.is_file() or module.name in actor_names:
            parser.error("Actor modules must be distinct files with unique names")
        actor_names.add(module.name)
        actor_modules.append(copy_actor_module(module, actor_directory, bundles / "actors"))
    port = smoke.unused_port()
    prefs = {
        "marionette.port": port,
        "browser.shell.checkDefaultBrowser": False,
        "browser.startup.homepage_override.mstone": "ignore",
        "browser.startup.page": 0,
        "browser.aboutwelcome.enabled": False,
        "floorp.releaseNotes.mode": "disabled",
        "floorp.releaseNotes.choiceConfirmed": True,
        "floorp.releaseNotes.promptShown": True,
        "floorp.browser.welcome.page.shown": True,
        "datareporting.policy.dataSubmissionEnabled": False,
        "browser.warnOnQuit": False,
    }
    certificates = []
    network_report = {"mode": "browser-default"}
    if args.environment_proxy:
        network_spec = importlib.util.spec_from_file_location(
            "private_profile_network", ROOT / "tools/chrome-extension-runtime/private-profile-network.py")
        network = importlib.util.module_from_spec(network_spec)
        network_spec.loader.exec_module(network)
        network_preferences, certificates, network_report = network.environment_network(os.environ)
        prefs.update(network_preferences)
    (profile / "user.js").write_text("\n".join(
        f"user_pref({json.dumps(key)}, {json.dumps(value)});"
        for key, value in prefs.items()) + "\n")
    run_id = uuid.uuid4().hex
    report = {"passed": False, "runId": run_id, "browser": str(binary), "modules": [], "networkEnvironment": network_report,
              "actorModules": actor_modules,
              "buildScope": "authenticated-native-candidate" if candidate is not None else "unqualified-browser-run"}
    if candidate is not None:
        report["candidate"] = candidate
    client = host = None
    fixture_handler = partial(FixtureHandler, directory=str(
        ROOT / "tools/chrome-extension-runtime/fixtures"))
    fixture_server = ThreadingHTTPServer(("127.0.0.1", 0), fixture_handler)
    fixture_server.user_scripts_barriers = UserScriptsParserBarriers()
    fixture_thread = threading.Thread(target=fixture_server.serve_forever, daemon=True)
    fixture_thread.start()
    fixture_url = f"http://127.0.0.1:{fixture_server.server_port}/"
    with (output / "browser.log").open("w") as log:
        try:
            verify_execution_sources(test_provenance, bundles, actor_modules)
            command = [str(binary), "--no-remote", "--profile", str(profile),
                       "--marionette", "--remote-allow-system-access"]
            if not args.headed:
                command.append("--headless")
            command.append("about:blank")
            browser_env = {**os.environ, "MOZ_CRASHREPORTER_DISABLE": "1",
                           "FLOORP_TEST_RUN_ID": run_id,
                           "FLOORP_TEST_REQUIRE_NATIVE_CORPUS": "1" if candidate is not None else "0",
                           "FLOORP_TEST_NATIVE_CANDIDATE_IDENTITY": json.dumps(candidate_provenance),
                           "FLOORP_TEST_ACTOR_DIRECTORY": str(actor_directory) if args.actor_module else ""}
            if candidate is not None:
                browser_env.pop("MOZ_DISABLE_CONTENT_SANDBOX", None)
            host = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                    env=browser_env)

            def connect():
                if host.poll() is not None:
                    raise RuntimeError(f"Browser exited {host.returncode}")
                try:
                    return smoke.Marionette(port)
                except (ConnectionRefusedError, socket.timeout):
                    return None

            client = smoke.wait_for("Marionette", connect, 45)
            client.context("chrome")
            if certificates:
                imported = client.script("""
                    const certdb = Cc['@mozilla.org/security/x509certdb;1'].getService(Ci.nsIX509CertDB);
                    for (const certificate of arguments[0]) {
                        certdb.addCertFromBase64(certificate, 'C,,');
                    }
                    return arguments[0].length;
                """, certificates)
                report["networkEnvironment"]["importedPrivateCertificates"] = imported
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
            if candidate is not None:
                candidate_identity.verify_runtime_identity(candidate, report["runtime"])
                report["nativeCandidateIdentityMatches"] = True
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
                report["modules"].append({
                    "path": str(test),
                    **test_provenance[index],
                    **result,
                })
                print(f"{'PASS' if result.get('passed') else 'FAIL'} {test}", flush=True)
            verify_execution_sources(test_provenance, bundles, actor_modules)
            report["passed"] = all(result["passed"] for result in report["modules"])
        except Exception as error:
            report["error"] = str(error)
        finally:
            fixture_server.user_scripts_barriers.close()
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
