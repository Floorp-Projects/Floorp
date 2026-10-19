#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run unchanged Chrome-measured Port state programs on a real native candidate.

Reuses the strict Native Messaging gate's owned OS registration and launcher.
Runs separately so its original four-host EOF/argv corpus remains unchanged.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
import uuid

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("floorp_native_registration", HERE / "test-native-messaging-candidate.py")
registration_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(registration_module)
PROBES = {
    "portStateStressProbeSource": ("native-port-state-stress-probe.js", "d62a9785e9cbd2dab40ee11d72752a0629bf7b385b35eb434936dc77e997dcff"),
    "incomingPortStateProbeSource": ("native-port-state-incoming-probe.js", "2b2b6ad2b64a5d7b3894f0288e653ebd26be4ce8107624ddb334937b05870f1d"),
}
HOST_SHA = "a69730066a6808dd1ba9353a9fb3cc75e2ee8937bcb5006d82af9f01c4070fa7"


def verify_exact_reference_sources():
    module = (HERE / "native-port-state-probe-sources.mjs").read_text(encoding="utf-8")
    for constant, (filename, expected) in PROBES.items():
        source = (HERE / filename).read_bytes()
        if hashlib.sha256(source).hexdigest() != expected:
            raise RuntimeError("Exact Chrome-measured program changed: " + filename)
        match = re.search(r"export const " + constant + r'\s*=\s*("(?:[^"\\]|\\.)*")\s*;', module)
        if not match or json.loads(match.group(1)).encode("utf-8") != source:
            raise RuntimeError("Bundled string differs from exact Chrome-measured program: " + filename)
    if hashlib.sha256((HERE / "native-port-state-candidate-host.py").read_bytes()).hexdigest() != HOST_SHA:
        raise RuntimeError("Owned echo/delayed-close host differs from exact Chrome-measured host")


def verify_host_evidence(path):
    events = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]
    starts = [event for event in events if event["event"] == "start"]
    endings = [event for event in events if event["event"] in ("eof", "delayed-exit")]
    if len(starts) != 4 or len(endings) != 4 or len(events) != 8:
        raise RuntimeError("Expected exactly four independent allowed hosts with four recorded endings")
    pids = {event["pid"] for event in starts}
    if len(pids) != 4 or {event["pid"] for event in endings} != pids:
        raise RuntimeError("Owned stress host process endings do not match exact starts")
    if sum(event["event"] == "eof" for event in endings) != 2 or sum(event["event"] == "delayed-exit" for event in endings) != 2:
        raise RuntimeError("Expected two echo EOFs and two intentional delayed native host exits")
    return {"started": 4, "ended": 4, "traffic": registration_module.file_record(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", type=Path, required=True)
    parser.add_argument("--candidate-manifest", type=Path, default=Path(os.environ["FLOORP_TEST_CANDIDATE_MANIFEST"]) if os.environ.get("FLOORP_TEST_CANDIDATE_MANIFEST") else None)
    parser.add_argument("--allow-unqualified", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    platform = registration_module.PLATFORMS.get(sys.platform)
    if platform is None:
        parser.error("Actual Linux, Windows or macOS required")
    if not args.candidate_manifest and (not args.allow_unqualified or os.environ.get("CI")):
        parser.error("Authenticated candidate required; local raw builds need --allow-unqualified")
    binary = args.browser.resolve(strict=True)
    manifest = args.candidate_manifest.resolve(strict=True) if args.candidate_manifest else None
    verify_exact_reference_sources()
    if args.output:
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=False)
    else:
        parent = registration_module.ROOT / "_dist/chrome-runtime-tests"
        parent.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="native-port-state-", dir=parent))
    files = [Path(__file__).resolve(), Path(spec.origin).resolve(), registration_module.HERE / "run-browser-tests.py", HERE / "NativePortStateCandidate.test.mts", HERE / "native-port-state-test-types.ts", HERE / "native-port-state-probe-sources.mjs", HERE / "native-port-state-candidate-host.py", *(HERE / value[0] for value in PROBES.values())]
    sources = [registration_module.file_record(path) for path in files]
    command, _ = registration_module.host_launcher(output, platform)
    deployed_script = output / "native-messaging-candidate-host.py"
    shutil.copyfile(HERE / "native-port-state-candidate-host.py", deployed_script)
    forbidden_directory = output / "forbidden"
    forbidden_directory.mkdir()
    forbidden_command, _ = registration_module.host_launcher(forbidden_directory, platform)
    forbidden_script = forbidden_directory / "native-messaging-candidate-host.py"
    forbidden_script.write_text("from pathlib import Path\nPath(__file__).with_name('forbidden-host-launched').write_text('started')\n", encoding="utf-8")
    deployed = [registration_module.file_record(path) for path in (deployed_script, command, forbidden_script, forbidden_command)]
    token = uuid.uuid4().hex
    hosts = {"allowed": "floorp_state_allowed_" + token, "forbidden": "floorp_state_forbidden_" + token, "missing": "floorp_state_missing_" + token}
    registrations = [registration_module.Registration(platform, output, hosts["allowed"], command), registration_module.Registration(platform, output, hosts["forbidden"], forbidden_command)]
    report = {"stage": "authored-actual-port-local-state-stress", "passed": False, "qualifiedNativeCandidate": False, "platform": platform, "token": token, "sources": sources, "deployedHost": deployed, "startedAt": time.time()}
    try:
        for registration in registrations:
            registration.register()
        # This manifest is newly created and belongs only to this wrapper.
        # Verify its identity before changing our own allowed_origins to deny the caller.
        forbidden = registrations[1]
        if registration_module.file_record(forbidden.manifest) != forbidden.registration_record["manifest"]:
            raise RuntimeError("Owned forbidden registration changed before origin update")
        value = json.loads(forbidden.manifest.read_text(encoding="utf-8"))
        value["allowed_origins"] = ["chrome-extension://" + "b" * 32 + "/"]
        forbidden.manifest.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
        forbidden.registration_record["manifest"] = registration_module.file_record(forbidden.manifest)
        configuration = {"token": token, "platform": platform, "hosts": hosts, "outputPath": str(output / "api-evidence.json"), "hostEventsPath": str(output / "allowed-host-events.jsonl"), "forbiddenLaunchPath": str(forbidden_directory / "forbidden-host-launched")}
        config_path = output / "configuration.json"
        config_path.write_text(json.dumps(configuration, indent=2) + "\n", encoding="utf-8")
        report["configuration"] = registration_module.file_record(config_path)
        environment = {**os.environ, **registrations[0].environment, "FLOORP_PORT_STATE_CANDIDATE_CONFIG": str(config_path)}
        environment.pop("MOZ_DISABLE_CONTENT_SANDBOX", None)
        arguments = [sys.executable, str(registration_module.HERE / "run-browser-tests.py"), "--browser", str(binary), "--test", str(HERE / "NativePortStateCandidate.test.mts"), "--output", str(output / "browser"), "--timeout", str(args.timeout)]
        if manifest:
            arguments.extend(["--candidate-manifest", str(manifest)])
        status = registration_module.run_browser(arguments, environment, args.timeout + 120)
        report["browserExitCode"] = status
        browser_reports = list((output / "browser").glob("run-*/report.json"))
        if len(browser_reports) != 1:
            raise RuntimeError("Expected exactly one actual native browser execution")
        browser_report = json.loads(browser_reports[0].read_text(encoding="utf-8"))
        report["browserReport"] = registration_module.file_record(browser_reports[0])
        for key in ("runId", "runtime", "buildScope"):
            report[key] = browser_report[key]
        if manifest:
            if browser_report["buildScope"] != "authenticated-native-candidate" or not browser_report.get("candidate"):
                raise RuntimeError("Browser lacks authenticated native candidate provenance")
            report["candidate"] = browser_report["candidate"]
        api_path = output / "api-evidence.json"
        api = json.loads(api_path.read_text(encoding="utf-8"))
        report["apiEvidence"] = registration_module.file_record(api_path)
        if api.get("passed") is not True or api.get("runId") != browser_report["runId"] or api.get("token") != token:
            raise RuntimeError("Stress API evidence failed or is not bound to this browser execution: " + str(api.get("error")))
        if len(api["states"]) != 2 or len(api["incoming"]) != 2:
            raise RuntimeError("Exact two-realm stress and incoming matrices incomplete")
        report["hostProcesses"] = verify_host_evidence(output / "allowed-host-events.jsonl")
        if (forbidden_directory / "forbidden-host-launched").exists():
            raise RuntimeError("Forbidden host was executed")
        if sources != [registration_module.file_record(path) for path in files] or deployed != [registration_module.file_record(Path(item["path"])) for item in deployed]:
            raise RuntimeError("Stress source or deployed executable bytes changed")
        if report["configuration"] != registration_module.file_record(config_path):
            raise RuntimeError("Stress configuration changed")
        for registration in registrations:
            if registration.registration_record["manifest"] != registration_module.file_record(registration.manifest):
                raise RuntimeError("Owned registration changed during browser execution")
        verify_exact_reference_sources()
        if status or browser_report.get("passed") is not True:
            raise RuntimeError("Actual native browser stress suite failed")
        report["passed"] = True
        report["qualifiedNativeCandidate"] = manifest is not None
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["registrations"] = [registration.registration_record for registration in registrations]
        cleanup_errors = []
        for registration in reversed(registrations):
            try:
                registration.cleanup()
            except Exception as error:
                cleanup_errors.append(str(error))
        report["registrationCleanupComplete"] = all(registration.cleanup_complete for registration in registrations)
        if cleanup_errors:
            report["cleanupError"] = "; ".join(cleanup_errors)
            report["passed"] = report["qualifiedNativeCandidate"] = False
        report["finishedAt"] = time.time()
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "qualifiedNativeCandidate": report["qualifiedNativeCandidate"], "report": str(output / "report.json"), "error": report.get("error"), "cleanupError": report.get("cleanupError")}, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
