#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run exact Chrome-measured public message programs on a genuine native build."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import time
import uuid

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("owned_browser_launcher", HERE / "test-native-messaging-candidate.py")
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
SOURCE_RECORDS = json.loads((HERE / "response-race-source-records.json").read_text(encoding="utf-8"))
EXPECTED_SOURCE_HASHES = {'responseRaceProbeSource': 'dfd50501aa1125b2b3e157ce9e972027d082c73ace75e32589fa3e5f08719083', 'responseRaceReferenceSource': '5d17f05783b99077ee7c1a2ee6f4d3395fcd502bc4e2cd30da830af7cccbd857'}


def verify_sources():
    module = (HERE / "native-response-race-sources.mjs").read_text(encoding="utf-8")
    if set(SOURCE_RECORDS) != set(EXPECTED_SOURCE_HASHES):
        raise RuntimeError("Measured Chrome probe/reference source matrix changed")
    for constant, item in SOURCE_RECORDS.items():
        data = (HERE / item["file"]).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != item["sha256"] or digest != EXPECTED_SOURCE_HASHES[constant]:
            raise RuntimeError("Measured Chrome source/reference changed: " + item["file"])
        match = re.search(r"export const " + constant + r'\s*=\s*("(?:[^"\\]|\\.)*")\s*;', module)
        if not match or json.loads(match.group(1)).encode("utf-8") != data:
            raise RuntimeError("Bundled constant differs from exact measured file: " + item["file"])


def expected_groups():
    return [(script_type + "/response-races", 12) for script_type in ("classic", "module")]



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", type=Path, required=True)
    parser.add_argument("--candidate-manifest", type=Path, default=Path(os.environ["FLOORP_TEST_CANDIDATE_MANIFEST"]) if os.environ.get("FLOORP_TEST_CANDIDATE_MANIFEST") else None)
    parser.add_argument("--allow-unqualified", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args()
    if not args.candidate_manifest and (not args.allow_unqualified or os.environ.get("CI")):
        parser.error("Authenticated candidate required; local raw builds need --allow-unqualified")
    binary = args.browser.resolve(strict=True)
    candidate = args.candidate_manifest.resolve(strict=True) if args.candidate_manifest else None
    verify_sources()
    if args.output:
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=False)
    else:
        parent = base.ROOT / "_dist/chrome-runtime-tests"
        parent.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="native-response-race-", dir=parent))
    own_names = ["NativeMessageResponseRaceCandidate.test.mts", "native-response-race-test-types.ts", "native-response-race-controller.mjs", "native-response-race-sources.mjs", "response-race-source-records.json", *(item["file"] for item in SOURCE_RECORDS.values())]
    paths = [Path(__file__).resolve(), Path(spec.origin).resolve(), base.HERE / "run-browser-tests.py", *(HERE / name for name in own_names)]
    sources = [base.file_record(path) for path in paths]
    token = uuid.uuid4().hex
    config_path = output / "configuration.json"
    config_path.write_text(json.dumps({"token": token, "outputPath": str(output / "api-evidence.json")}, indent=2) + "\n", encoding="utf-8")
    configuration = base.file_record(config_path)
    report = {"stage": "authored-actual-native-response-race", "passed": False, "qualifiedNativeCandidate": False, "token": token, "sources": sources, "configuration": configuration, "startedAt": time.time()}
    try:
        environment = {**os.environ, "FLOORP_RESPONSE_RACE_CANDIDATE_CONFIG": str(config_path)}
        environment.pop("MOZ_DISABLE_CONTENT_SANDBOX", None)
        arguments = [sys.executable, str(base.HERE / "run-browser-tests.py"), "--browser", str(binary), "--test", str(HERE / "NativeMessageResponseRaceCandidate.test.mts"), "--output", str(output / "browser"), "--timeout", str(args.timeout)]
        if candidate:
            arguments.extend(["--candidate-manifest", str(candidate)])
        status = base.run_browser(arguments, environment, args.timeout + 120)
        report["browserExitCode"] = status
        reports = list((output / "browser").glob("run-*/report.json"))
        if len(reports) != 1:
            raise RuntimeError("Expected exactly one actual browser execution")
        browser = json.loads(reports[0].read_text(encoding="utf-8"))
        report["browserReport"] = base.file_record(reports[0])
        for key in ("runId", "runtime", "buildScope"):
            report[key] = browser[key]
        if candidate:
            if browser["buildScope"] != "authenticated-native-candidate" or not browser.get("candidate"):
                raise RuntimeError("Browser lacks authenticated native candidate provenance")
            report["candidate"] = browser["candidate"]
        api_path = output / "api-evidence.json"
        api = json.loads(api_path.read_text(encoding="utf-8"))
        report["apiEvidence"] = base.file_record(api_path)
        report["groups"] = [{"name": item["name"], "observed": len(item["observations"]), "failures": len(item["failures"])} for item in api["groups"]]
        if api.get("token") != token or api.get("runId") != browser["runId"]:
            raise RuntimeError("Public API observations do not belong to this browser execution")
        actual_groups = [(item["name"], item["expectedCount"]) for item in api["groups"]]
        if actual_groups != expected_groups():
            raise RuntimeError("Exact classic/module response-race group matrix incomplete")
        if any(len(item["observations"]) != item["expectedCount"] or item["failures"] for item in api["groups"]):
            raise RuntimeError("Public Chrome/Firefox message contract mismatch; see aggregate api-evidence.json")
        if api.get("passed") is not True or api.get("cleanupErrors"):
            raise RuntimeError("Public message API gate or owned extension cleanup failed")
        if sources != [base.file_record(path) for path in paths] or configuration != base.file_record(config_path):
            raise RuntimeError("Execution sources or configuration changed")
        verify_sources()
        if status or browser.get("passed") is not True:
            raise RuntimeError("Actual native browser suite failed")
        report["passed"] = True
        report["qualifiedNativeCandidate"] = candidate is not None
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["finishedAt"] = time.time()
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "qualifiedNativeCandidate": report["qualifiedNativeCandidate"], "report": str(output / "report.json"), "error": report.get("error")}, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
