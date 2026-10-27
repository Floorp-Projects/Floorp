#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Prove actual quiet native host lifetime against exact public Chrome probes."""
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
spec = importlib.util.spec_from_file_location("quiet_native_registration", HERE / "test-native-messaging-candidate.py")
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
PROBES = {
    "quietNativeWorkerProbeSource": ("quiet-native-worker-probe.js.txt", "bdbb9a97d9000ca1c9411e75ad72ae53ec1719c599ff2cbb6aea9df2bfefef8f"),
    "quietNativeDocumentProbeSource": ("quiet-native-document-probe.js.txt", "c03bb528d43c7b85b5eeb4887cfab1460df9b305c4583506dd07458651290dcf"),
}
HOST_SHA256 = "7771303939e19651810117130b7e759950af61ea94069c587b3a3c9d1026392e"


def verify_sources():
    module = (HERE / "quiet-native-probe-sources.mjs").read_text(encoding="utf-8")
    for constant, (name, expected) in PROBES.items():
        data = (HERE / name).read_bytes()
        match = re.search(r"export const " + constant + r'\s*=\s*("(?:[^"\\]|\\.)*")\s*;', module)
        if hashlib.sha256(data).hexdigest() != expected or not match or json.loads(match.group(1)).encode("utf-8") != data:
            raise RuntimeError("Exact Chrome public probe bytes changed: " + name)
    if base.file_record(base.HERE / "native-messaging-candidate-host.py")["sha256"] != HOST_SHA256:
        raise RuntimeError("Exact Chrome reference framed host bytes changed")


def verify_hosts(path):
    events = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]
    starts = [row for row in events if row["event"] == "start"]
    exits = [row for row in events if row["event"] == "exit"]
    if len(starts) != 2 or len({row["pid"] for row in starts}) != 2:
        raise RuntimeError("Only the two native cases may launch actual hosts")
    if len(exits) != 2 or any(row.get("clean") is not True or row.get("messages") != 2 for row in exits):
        raise RuntimeError("Both actual hosts must close cleanly after exactly two frames")
    for start in starts:
        pid = start["pid"]
        received = [row for row in events if row["event"] == "receive" and row["pid"] == pid]
        if [row["serial"] for row in received] != [1, 2] or [row["message"]["command"] for row in received] != ["begin", "probe"]:
            raise RuntimeError("Actual host framing/quiet probe sequence changed")
        if not any(row["pid"] == pid for row in exits):
            raise RuntimeError("Owned host has no clean EOF outcome")
    return {"processes": 2, "cleanExits": 2, "frames": 4, "traffic": base.file_record(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", type=Path, required=True)
    parser.add_argument("--expected-build-id", required=True)
    parser.add_argument("--candidate-manifest", type=Path)
    parser.add_argument("--allow-unqualified", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--timeout", type=int, default=240)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9]{14}", args.expected_build_id):
        parser.error("Expected the exact 14-digit root-authorized native BuildID")
    if not args.candidate_manifest and (not args.allow_unqualified or os.environ.get("CI")):
        parser.error("CI requires an authenticated candidate; raw local builds need --allow-unqualified")
    platform = base.PLATFORMS.get(sys.platform)
    if platform is None:
        parser.error("Actual Linux, Windows or macOS is required")
    verify_sources()
    output = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix="quiet-native-lifetime-"))
    if args.output:
        output.mkdir(parents=True, exist_ok=False)
    binary = args.browser.resolve(strict=True)
    candidate = args.candidate_manifest.resolve(strict=True) if args.candidate_manifest else None
    paths = [Path(__file__).resolve(), Path(spec.origin).resolve(), base.HERE / "run-browser-tests.py", base.HERE / "native-messaging-candidate-host.py", HERE / "QuietNativePortLifetime.test.mts", HERE / "quiet-native-lifetime-types.ts", HERE / "quiet-native-probe-sources.mjs", *(HERE / value[0] for value in PROBES.values())]
    sources = [base.file_record(path) for path in paths]
    token = uuid.uuid4().hex
    command, deployed = base.host_launcher(output, platform)
    registration = base.Registration(platform, output, "floorp_quiet_" + token, command)
    report = {"stage": "authored-real-native-port-quiet-lifetime", "passed": False, "qualifiedNativeCandidate": False, "token": token, "sources": sources, "deployedHost": deployed, "startedAt": time.time()}
    try:
        host_manifest = registration.register()
        config = {"hostName": registration.host_name, "manifestPath": str(host_manifest), "hostPath": str(command), "trafficPath": str(output / "host-traffic.jsonl"), "outputPath": str(output / "api-evidence.json"), "token": token, "platform": platform, "quietMilliseconds": 36000, "expectedBuildId": args.expected_build_id}
        config_path = output / "configuration.json"
        config_path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
        configuration = base.file_record(config_path)
        report["configuration"] = configuration
        environment = {**os.environ, **registration.environment, "FLOORP_QUIET_NATIVE_LIFETIME_CONFIG": str(config_path)}
        for name in ("MOZ_DISABLE_CONTENT_SANDBOX", "MOZ_HEADLESS", "FLOORP_TEST_CANDIDATE_MANIFEST", "FLOORP_CANDIDATE_MANIFEST"):
            environment.pop(name, None)
        arguments = [sys.executable, str(base.HERE / "run-browser-tests.py"), "--browser", str(binary), "--test", str(HERE / "QuietNativePortLifetime.test.mts"), "--output", str(output / "browser"), "--timeout", str(args.timeout)]
        if candidate:
            arguments.extend(["--candidate-manifest", str(candidate)])
        status = base.run_browser(arguments, environment, args.timeout + 120)
        report["browserExitCode"] = status
        reports = list((output / "browser").glob("run-*/report.json"))
        if len(reports) != 1:
            raise RuntimeError("Expected exactly one actual browser report")
        browser = json.loads(reports[0].read_text(encoding="utf-8"))
        report["browserReport"] = base.file_record(reports[0])
        for key in ("runId", "runtime", "buildScope"):
            report[key] = browser[key]
        if browser["runtime"]["buildId"] != args.expected_build_id:
            raise RuntimeError("Actual native build is not the root-authorized BuildID")
        if candidate:
            if browser["buildScope"] != "authenticated-native-candidate" or not browser.get("candidate"):
                raise RuntimeError("Actual browser lacks authenticated production provenance")
            report["candidate"] = browser["candidate"]
        api_path = output / "api-evidence.json"
        api = json.loads(api_path.read_text(encoding="utf-8"))
        report["apiEvidence"] = base.file_record(api_path)
        report["cases"] = api.get("cases")
        if api.get("runId") != browser["runId"] or api.get("token") != token:
            raise RuntimeError("Public observations do not belong to this exact browser run")
        if [(row["scriptType"], row["native"]) for row in api["cases"]] != [(kind, native) for kind in ("classic", "module") for native in (True, False)]:
            raise RuntimeError("Exact classic/module native/ordinary matrix incomplete")
        if api.get("passed") is not True or any(row.get("passed") is not True for row in api["cases"]) or api.get("cleanupErrors"):
            raise RuntimeError("Actual quiet native/ordinary Port lifetime contract failed")
        report["hostProcesses"] = verify_hosts(output / "host-traffic.jsonl")
        if status or browser.get("passed") is not True:
            raise RuntimeError("Actual native browser suite failed")
        if sources != [base.file_record(path) for path in paths] or deployed != [base.file_record(Path(row["path"])) for row in deployed] or configuration != base.file_record(config_path):
            raise RuntimeError("Execution sources, deployed host or owned configuration changed")
        verify_sources()
        report["passed"] = True
        report["qualifiedNativeCandidate"] = candidate is not None
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["registration"] = registration.registration_record
        try:
            registration.cleanup()
        except Exception as error:
            report["cleanupError"] = str(error)
            report["passed"] = report["qualifiedNativeCandidate"] = False
        report["registrationCleanupComplete"] = registration.cleanup_complete
        report["finishedAt"] = time.time()
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "report": str(output / "report.json"), "error": report.get("error"), "cleanupError": report.get("cleanupError")}, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
