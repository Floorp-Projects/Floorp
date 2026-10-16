# SPDX-License-Identifier: MPL-2.0
"""Observe one completed provider job. Never issue publication/Agent qualification."""
import argparse
import hashlib
import json
import math
import re
import stat
import sys
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/linux-package-qa"))
from intake import freeze_archive, read_once, unique_object  # noqa: E402

RUNNER = "blacksmith-8vcpu-ubuntu-2404"
JOB = "Final package QA / linux/x86_64"
PROBES = {
    "web-panel-ua": "Verify shipped Web Panel HTTP User-Agent behavior",
    "fluent-flasco": "Verify shipped Fluent lookup and Flasco context-menu availability",
}
LIMIT = 64 * 1024**2


def require(condition, message):
    if not condition:
        raise ValueError(message)


def positive(value):
    return type(value) is int and value > 0


def json_bytes(data):
    require(len(data) <= 2 * 1024**2, "JSON exceeds byte bound")
    return json.loads(data, object_pairs_hook=unique_object,
                      parse_float=finite_float,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))


def finite_float(value):
    number = float(value)
    require(math.isfinite(number), "Nonfinite JSON number")
    return number


def one(values, predicate, message):
    found = [value for value in values if predicate(value)]
    require(len(found) == 1, message)
    return found[0]


def artifact_matches(artifact, expected, run):
    return (artifact.get("id") == expected.get("id") and positive(artifact.get("id")) and
            positive(expected.get("id")) and
            artifact.get("name") == expected.get("name") and
            artifact.get("digest") == expected.get("digest") and
            re.fullmatch(r"sha256:[0-9a-f]{64}", artifact.get("digest", "")) and
            artifact.get("expired") is False and
            artifact.get("workflow_run", {}).get("id") == run["id"] and
            artifact["workflow_run"].get("head_sha") == run["head_sha"])


def read_archive(path, output, artifact):
    require(positive(artifact.get("id")), "Missing original raw artifact")
    frozen = output / "original-evidence.zip"
    freeze_archive(path, frozen, LIMIT)
    with frozen.open("rb") as stream:
        digest = "sha256:" + hashlib.file_digest(stream, "sha256").hexdigest()
    require(digest == artifact.get("digest"), "Original evidence archive digest mismatch")
    retained, inventory = {}, {}
    with zipfile.ZipFile(frozen) as archive:
        entries = archive.infolist()
        require(0 < len(entries) <= 100, "Evidence archive member count rejected")
        names, total = set(), 0
        for info in entries:
            name = info.filename
            path = PurePosixPath(name)
            require(name not in names and not path.is_absolute() and
                    ".." not in path.parts and "\\" not in name and
                    str(path) == name.rstrip("/"), "Unsafe or duplicate evidence member")
            names.add(name)
            require(not stat.S_ISLNK(info.external_attr >> 16) and not info.flag_bits & 1,
                    "Symlink/encrypted evidence member rejected")
            if info.is_dir():
                continue
            total += info.file_size
            require(0 <= info.file_size <= 16 * 1024**2 and total <= LIMIT,
                    "Evidence archive byte bound rejected")
            hasher, chunks, count = hashlib.sha256(), [], 0
            keep = name in {"input.json", "native-verification.json"} or bool(
                re.fullmatch(r"(?:web-panel-ua|fluent-flasco)/run-[^/]+/report.json", name))
            with archive.open(info) as stream:
                while chunk := stream.read(65536):
                    count += len(chunk)
                    require(count <= info.file_size and count <= 16 * 1024**2,
                            "Evidence member grew beyond bound")
                    hasher.update(chunk)
                    if keep:
                        require(count <= 2 * 1024**2, "Report exceeds byte bound")
                        chunks.append(chunk)
            require(count == info.file_size, "Incomplete evidence member")
            inventory[name] = {"bytes": count, "sha256": hasher.hexdigest()}
            if keep:
                retained[name] = json_bytes(b"".join(chunks))
    return retained, inventory


def validate_report(report, record, case, inp):
    require(report.get("status") == "passed", "Original probe did not pass")
    require(report.get("nativeRecord") == record, "Probe refers to another native record")
    require(not any((key.lower().endswith("error") or key == "unexpectedHostExit") and value is not None
                    for key, value in report.items()), "Original probe retained an error")
    host = report.get("hostProcessIdentity", {})
    require(host.get("verified") is True and positive(host.get("pid")) and
            report.get("ownedHostPid") == host["pid"], "Owned host identity missing")
    signal = report.get("cleanupRequestedSignal")
    require(signal in (None, 15) and
            type(report.get("hostExitCode")) is int and
            report["hostExitCode"] in (0, -int(signal or 0)), "Unconfirmed or forced host cleanup")
    if case == "web-panel-ua":
        identity = report.get("identity", {})
        require(isinstance(report.get("checks"), list) and bool(report["checks"]),
                "Missing UA checks")
        redirects = report.get("redirectChecks", [])
        require(len(redirects) == 2 and {item.get("caseId") for item in redirects} == {
            "web-panel-ua.redirect.mobile-on", "web-panel-ua.redirect.mobile-off"} and
            all(item.get("status") == "passed" for item in redirects), "Incomplete UA redirects")
        require(report.get("httpFixtureError", "missing") is None, "HTTP fixture incomplete")
    else:
        regression = report.get("browserRegression", {})
        require(regression.get("status") == "passed" and
                isinstance(regression.get("checks"), list) and bool(regression["checks"]),
                "Missing Fluent/Flasco regression checks")
        identity = regression.get("identity", {})
    version = identity.get("version2" if case == "web-panel-ua" else "floorpVersion")
    require(version == inp["floorpVersion"] and
            identity.get("version") == record["firefox_version"] and
            identity.get("appBuildID") == identity.get("platformBuildID") == record["app_build_id"] and
            identity.get("buildID2") == record["build_id2"] and
            identity.get("startupMode") not in (None, "dev", "test") and
            identity.get("httpLoaderAllowed") is False and
            type(identity.get("sandboxLevel")) is int and identity["sandboxLevel"] > 0,
            "Shipped browser/loader/sandbox identity incomplete")


def collect(context, retained, inventory):
    result = {"schema": "qa3-linux-l2-observation-v1", "status": "BLOCK",
              "scope": "linux-x64-existing-final-two-probes", "publicationAuthorized": False,
              "globalFirstEvaluation": "UNVERIFIED", "agentQualification": "UNVERIFIED",
              "descendantAndPortCleanup": "UNVERIFIED", "secretIsolationQualification": "UNVERIFIED",
              "cases": [{"id": case, "status": "NOT_EXECUTED"} for case in PROBES],
              "errors": [], "evidence": inventory,
              "provider": context, "executedCases": 0, "passedCases": 0}
    errors = result["errors"]
    original_jobs = context.get("originalJobs")
    originals = [job for job in (original_jobs if isinstance(original_jobs, list) else [])
                 if isinstance(job, dict) and job.get("name") == JOB]
    if len(originals) == 1:
        result["originalJob"] = originals[0]
        original_steps = originals[0].get("steps")
        result["originalFailureSteps"] = [step for step in (original_steps if isinstance(original_steps, list) else [])
            if isinstance(step, dict) and step.get("conclusion") in {"failure", "timed_out", "cancelled"}]
    try:
        require(not context.get("collectionErrors"), "Provider snapshots incomplete")
        run, selection = context["run"], context["selection"]
        require(run["repository"]["full_name"] == "Floorp-Projects/Floorp" and
                run["path"] == ".github/workflows/validate-final-package-qa.yml" and
                run["event"] == "workflow_dispatch" and positive(run["id"]) and
                run["id"] == selection["executionRunId"] and
                run["head_sha"] == selection["probeHead"], "Consumer identity mismatch")
        original = one(context["originalJobs"], lambda job: job["name"] == JOB,
                       "Missing unique original native job")
        result["originalJob"] = original
        require(type(run["run_attempt"]) is int and type(selection["executionAttempt"]) is int and
                run["run_attempt"] == selection["executionAttempt"] == 1,
                "Retry cannot replace the original result")
        require(original.get("run_id") == run["id"] and type(original.get("run_attempt")) is int and
                original["run_attempt"] == 1 and
                positive(original.get("id")) and RUNNER in original.get("labels", []) and
                bool(original.get("runner_name")), "Original Blacksmith execution unverified")
        steps = original.get("steps", [])
        for name in ("Authenticate the normal package run and exact artifacts",
                     "Validate the native record and extract the exact package",
                     "Set up the Linux display and extract the package"):
            step = one(steps, lambda step: step["name"] == name, "Missing original intake step")
            require(step["status"] == "completed" and step["conclusion"] == "success",
                    "Original intake/display did not succeed")
        inp, record = retained["input.json"], retained["native-verification.json"]
        producer = context["packageRun"]
        require(producer["repository"]["full_name"] == "Floorp-Projects/Floorp" and
                producer["path"] == ".github/workflows/package_and_publish_release.yml" and
                producer["event"] == "workflow_dispatch" and type(producer["run_attempt"]) is int and
                producer["run_attempt"] == 1 and positive(producer["id"]) and
                inp["runId"] == producer["id"] == selection["packageRunId"] and
                type(inp["sourceAttempt"]) is int and inp["sourceAttempt"] == 1 and
                inp["floorpHead"] == producer["head_sha"] == selection["floorpHead"] and
                inp["probeHead"] == run["head_sha"] and inp["unsigned"] is False and
                inp["platform"] == "linux" and inp["arch"] == "x86_64", "Producer/subject mismatch")
        for kind in ("package", "record"):
            one(context["packageArtifacts"], lambda artifact: artifact_matches(artifact, inp[kind], producer),
                "Missing exact original product artifact")
        require(inp["package"]["name"] == "noraneko-linux-x86_64-installer" and
                inp["record"]["name"] == "floorp-native-verification-linux-x86_64-v2",
                "Unexpected original artifact kind")
        native = one(context["packageJobs"], lambda job: job["name"].endswith("Verify linux/x86_64 package"),
                     "Missing original Linux native verification job")
        require(native.get("run_id") == producer["id"] and type(native.get("run_attempt")) is int and
                native["run_attempt"] == 1 and
                native["status"] == "completed" and native["conclusion"] == "success",
                "Original native verification failed or missing")
        require(record["schema_version"] == 2 and record["verification"]["status"] == "verified" and
                record["floorp"]["workflow_run_id"] == producer["id"] and
                record["floorp"]["head_sha"] == producer["head_sha"] and
                record["floorp"]["repository"] == "Floorp-Projects/Floorp" and
                record["target_key"] == "linux" and record["native_arch"] == "x86_64" and
                record["firefox_version"] == inp["firefoxVersion"] and
                record["app_build_id"] == record["platform_build_id"] ==
                record["runtime"]["expected_build_id"] == inp["runtimeBuildId"] and
                record["floorp_package"] == {"artifact_id": inp["package"]["id"],
                    "artifact_name": inp["package"]["name"], "artifact_digest": inp["package"]["digest"],
                    "unsigned": False}, "Native record does not bind this product")
        result["subject"] = inp
        for item, (case, name) in zip(result["cases"], PROBES.items()):
            try:
                step = one(steps, lambda step: step["name"] == name, "Missing probe step")
                item["providerStep"] = step
                if step["conclusion"] in {"failure", "timed_out"}:
                    item["status"] = "FAIL"
                require(step["status"] == "completed" and step["conclusion"] == "success",
                        "Original probe step failed/skipped/incomplete")
                path = one(list(retained), lambda path: bool(re.fullmatch(
                    re.escape(case) + r"/run-[^/]+/report.json", path)), "Missing unique original report")
                item["report"] = path
                item["status"] = "FAIL"
                report = retained[path]
                if report.get("hostProcessIdentity", {}).get("verified") is True:
                    result["executedCases"] += 1
                validate_report(report, record, case, inp)
                require(inventory.get(path[:-11] + "browser.log", {}).get("bytes", 0) > 0,
                        "Missing original browser log")
                item["status"] = "PASS"
                result["passedCases"] += 1
            except (ValueError, KeyError, TypeError, AttributeError, IndexError) as error:
                item["error"] = str(error)
                errors.append(case + ": " + str(error))
        require(original["status"] == "completed" and original["conclusion"] == "success",
                "Original native job did not succeed")
        require(not errors and result["passedCases"] == 2, "Required probe evidence incomplete")
        result["status"] = "PASS_SCOPE_ONLY"
    except (ValueError, KeyError, TypeError, AttributeError, IndexError) as error:
        errors.append(str(error))
    if errors and (any(case["status"] == "FAIL" for case in result["cases"]) or
                   result.get("originalJob", {}).get("conclusion") in {"failure", "timed_out"}):
        result["status"] = "FAIL"
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--context", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(mode=0o700)
    context, retained, inventory = {}, {}, {}
    try:
        parsed = json_bytes(read_once(args.context, 2 * 1024**2))
        require(isinstance(parsed, dict), "Provider context must be an object")
        context = parsed
        artifact = context["rawArtifact"]
        require(artifact_matches(artifact, {"id": context["rawArtifactIdExpected"],
                "name": f"final-package-qa-linux-x86_64-{context['run']['id']}-1",
                "digest": artifact.get("digest")}, context["run"]), "Raw artifact identity mismatch")
        retained, inventory = read_archive(args.archive, args.output, artifact)
    except (OSError, ValueError, KeyError, TypeError, AttributeError, IndexError, zipfile.BadZipFile) as error:
        context.setdefault("collectionErrors", []).append(str(error))
    result = collect(context, retained, inventory)
    with (args.output / "observation.json").open("x") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
    print(json.dumps({"status": result["status"], "executedCases": result["executedCases"],
                      "publicationAuthorized": False}))
    return 0 if result["status"] == "PASS_SCOPE_ONLY" else 1


if __name__ == "__main__":
    raise SystemExit(main())
