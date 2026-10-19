#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run the maintained native PAC scope regression in the packaged XPCOM shell."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
TEST_PATH = "netwerk/test/unit/test_chrome_proxy_scopes.js"
MARKER = "FLOORP_NATIVE_PROXY_RESULT="


def maintained_test():
    patch = ROOT / ".github/patches/floorp-runtime/common/chrome-extension-proxy-scopes-native.patch"
    section = patch.read_text().split(f"diff --git a/{TEST_PATH} b/{TEST_PATH}\n", 1)
    if len(section) != 2:
        raise RuntimeError("The maintained native PAC regression is missing")
    section = section[1].split("\ndiff --git ", 1)[0]
    code = "\n".join(line[1:] for line in section.splitlines()
                     if line.startswith("+") and not line.startswith("+++")) + "\n"
    if code.count("add_task(") != 4:
        raise RuntimeError("The maintained native PAC regression changed; review the shell bootstrap")
    return code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path)
    args = parser.parse_args()
    binary = args.browser.resolve(strict=True)
    if not binary.is_file():
        raise ValueError("Pass the actual packaged browser executable")
    evidence = ROOT / "_dist/chrome-runtime-tests"
    evidence.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="native-proxy-", dir=evidence))
    profile = output / "profile"
    profile.mkdir()
    code = maintained_test()
    script = output / "native-proxy.js"
    script.write_text(r'''
"use strict";
const { NetUtil } = ChromeUtils.importESModule("resource://gre/modules/NetUtil.sys.mjs");
const { setTimeout } = ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");
const tests = [];
function add_task(task) { tests.push(task); }
function do_timeout(delay, callback) { return setTimeout(callback, delay); }
const Assert = {
  equal(actual, expected) {
    if (actual !== expected) throw new Error(`Expected ${expected}; received ${actual}`);
  },
  ok(value) { if (!value) throw new Error("Native assertion failed"); },
};
Services.io.manageOfflineStatus = false;
Services.io.offline = false;
''' + code + r'''
let finished = false;
const result = {passed:false, tests:[], runtime:{os:Services.appinfo.OS, abi:Services.appinfo.XPCOMABI}};
(async () => {
  for (const task of tests) {
    await task();
    result.tests.push({name:task.name, passed:true});
  }
  result.passed = result.tests.length === 4;
})().catch(error => { result.error = String(error); result.stack = error.stack; })
  .finally(() => { finished = true; });
Services.tm.spinEventLoopUntil("Floorp native proxy scope regression", () => finished);
print("FLOORP_NATIVE_PROXY_RESULT=" + JSON.stringify(result));
if (!result.passed) throw new Error(result.error || "Native PAC scope regression did not complete");
''')
    resources = binary.parent
    if binary.parent.name == "MacOS" and binary.parent.parent.name == "Contents":
        resources = binary.parent.parent / "Resources"
    command = [str(binary), "--xpcshell", "-g", str(resources), "-a", str(resources), "-m", "-f", str(script)]
    report = {"passed": False, "command": command,
              "testSha256": hashlib.sha256(code.encode()).hexdigest()}
    try:
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120,
                                env={**os.environ, "MOZ_HEADLESS": "1", "MOZ_CRASHREPORTER_DISABLE": "1",
                                     "MOZ_NO_REMOTE": "1", "XRE_PROFILE_PATH": str(profile),
                                     "XRE_PROFILE_LOCAL_PATH": str(profile), "XPCSHELL_TEST_PROFILE_DIR": str(profile)})
        (output / "xpcshell.log").write_text(result.stdout + result.stderr)
        reports = [line[len(MARKER):] for line in result.stdout.splitlines() if line.startswith(MARKER)]
        report["exitCode"] = result.returncode
        if len(reports) != 1:
            raise RuntimeError("Expected exactly one actual XPCOM-shell regression result")
        report["native"] = json.loads(reports[0])
        report["passed"] = result.returncode == 0 and report["native"].get("passed") is True
    except subprocess.TimeoutExpired as error:
        (output / "xpcshell.log").write_bytes((error.stdout or b"") + (error.stderr or b""))
        report["error"] = "The actual native PAC/XPCOM shell exceeded its two-minute bound"
    except Exception as error:
        report["error"] = str(error)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{'PASS' if report['passed'] else 'FAIL'} actual native PAC scope shell: {output / 'report.json'}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
