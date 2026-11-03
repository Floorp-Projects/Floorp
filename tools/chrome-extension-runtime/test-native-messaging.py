#!/usr/bin/env python3
"""Verify the pinned Runtime patch and exercise a real native host in Floorp.

The source fixture is reconstructed with git show at floorp-runtime.lock.json's
commit. Neither the sibling checkout nor the installed browser is modified.
The Linux integration test starts a private profile, spawns its own Python host,
and asserts Chrome launch arguments, framed JSON, authorization, and precedence.
"""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE_PATHS = [
    "toolkit/components/extensions/NativeManifests.sys.mjs",
    "toolkit/components/extensions/NativeMessaging.sys.mjs",
    "toolkit/components/extensions/schemas/manifest.json",
    "toolkit/components/extensions/test/xpcshell/test_native_manifests.js",
]


def run(args):
    runtime_source = args.runtime_source.resolve()
    binary = args.binary.resolve()
    lock = json.loads((ROOT / "floorp-runtime.lock.json").read_text())
    commit = lock["source"]["commit"]
    parent = ROOT / "_dist/chrome-runtime-tests"
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="native-messaging-", dir=parent))
    fixture = output / "source"
    for relative in SOURCE_PATHS:
        target = fixture / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(subprocess.check_output([
            "git", "-C", str(runtime_source), "show", f"{commit}:{relative}",
        ]))
    subprocess.run(["git", "init", "-q", str(fixture)], check=True)
    patch = ROOT / ".github/patches/floorp-runtime/common/chrome-extension-native-messaging.patch"
    for flags in [["--check"], []]:
        subprocess.run(["git", "apply", *flags, str(patch)], cwd=fixture, check=True)
    modules = fixture / "toolkit/components/extensions"
    messaging = modules / "NativeMessaging.sys.mjs"
    original_import = 'NativeManifests: "resource://gre/modules/NativeManifests.sys.mjs"'
    isolated_import = 'NativeManifests: "resource://floorp-native-fixture/NativeManifests.sys.mjs"'
    source = messaging.read_text()
    if source.count(original_import) != 1:
        raise RuntimeError("Pinned NativeMessaging module import changed")
    messaging.write_text(source.replace(original_import, isolated_import))
    schema = json.loads((modules / "schemas/manifest.json").read_text())
    gecko = next(value for namespace in schema for value in namespace.get("types", [])
                 if value.get("id") == "FirefoxSpecificProperties")
    metadata = {**gecko["properties"]["floorp_chrome_extension"], "id": "Metadata"}
    metadata.pop("optional", None)
    (modules / "metadata-schema.json").write_text(json.dumps([
        {"namespace": "floorpChromeManifest", "types": [metadata]},
    ]))

    spec = importlib.util.spec_from_file_location("floorp_smoke", ROOT / "tools/app-shim/test-runtime.py")
    smoke = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(smoke)
    profile = output / "profile"
    profile.mkdir()
    port = smoke.unused_port()
    preferences = {
        "marionette.port": port,
        "browser.shell.checkDefaultBrowser": False,
        "browser.startup.homepage_override.mstone": "ignore",
        "browser.startup.page": 0,
        "browser.aboutwelcome.enabled": False,
        "datareporting.policy.dataSubmissionEnabled": False,
        "widget.use-xdg-desktop-portal.native-messaging": 0,
        "widget.use-xdg-desktop-portal.native-messaging-proxy": 0,
    }
    (profile / "user.js").write_text("".join(
        f"user_pref({json.dumps(key)}, {json.dumps(value)});\n"
        for key, value in preferences.items()
    ))
    client = None
    result = {"passed": False, "runtimeSourceCommit": commit}
    with (output / "browser.log").open("w") as log:
        browser = subprocess.Popen([
            str(binary), "--headless", "--no-remote", "--profile", str(profile),
            "--marionette", "--remote-allow-system-access", "about:blank",
        ], stdout=log, stderr=subprocess.STDOUT,
           env={**os.environ, "MOZ_CRASHREPORTER_DISABLE": "1"})
        try:
            def connect():
                if browser.poll() is not None:
                    raise RuntimeError(f"Browser exited with status {browser.returncode}")
                try:
                    return smoke.Marionette(port)
                except (ConnectionRefusedError, socket.timeout):
                    return None

            client = smoke.wait_for("Marionette", connect, 45)
            client.context("chrome")
            client.script("""
              const [fixture, harness, python] = arguments;
              const handler = Services.io.getProtocolHandler('resource')
                .QueryInterface(Ci.nsIResProtocolHandler);
              for (const [name, path] of [
                ['floorp-native-fixture', fixture],
                ['floorp-native-harness', harness],
              ]) {
                const directory = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
                directory.initWithPath(path);
                handler.setSubstitution(name, Services.io.newFileURI(directory));
              }
              const state = window.wrappedJSObject;
              state.__floorpNativeTest = { done: false };
              try {
                const tests = ChromeUtils.importESModule(
                  'resource://floorp-native-harness/native-messaging-test.sys.mjs');
                tests.runTests(fixture, python).then(result => {
                  state.__floorpNativeTest = { done: true, ...result };
                }, error => {
                  state.__floorpNativeTest = { done: true, passed: false,
                    error: String(error), stack: error.stack };
                });
              } catch (error) {
                state.__floorpNativeTest = { done: true, passed: false,
                  error: String(error), stack: error.stack };
              }
            """, str(modules), str(Path(__file__).resolve().parent), sys.executable)
            result.update(smoke.wait_for("Native messaging tests", lambda: client.script(
                "return window.wrappedJSObject.__floorpNativeTest.done ? "
                "window.wrappedJSObject.__floorpNativeTest : null;"
            ), 45))
        except Exception as error:
            result["error"] = str(error)
        finally:
            if client:
                client.close()
            browser.terminate()
            try:
                browser.wait(timeout=8)
            except subprocess.TimeoutExpired:
                browser.kill()
                browser.wait(timeout=5)
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    print(output / "report.json")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-source", type=Path, default=ROOT.parent / "Floorp-Runtime")
    parser.add_argument("--binary", type=Path, default=ROOT / "_dist/bin/floorp/floorp")
    raise SystemExit(run(parser.parse_args()))
