# SPDX-License-Identifier: MPL-2.0

import argparse
import hashlib
import json
import os
import platform
import re
import signal
import subprocess
import sys
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GROUPS = {
    "deno-host": (
        "tools/src/smoke_runner.test.ts",
        "tools/src/utils.test.ts",
        "tools/src/colocated_test_runner.test.ts",
    ),
    "node-intake": ("tools/test-final-package-qa-source.mjs",),
    "python-provenance": (
        "tools/test_validate_runtime_source_provenance.py",
        "tools/test_validate_runtime_packaging_provenance.py",
    ),
}
CASE_COUNTS = {"deno-host": 60, "node-intake": 28, "python-provenance": 18}
TARGETS = [
    "windows-x86_64",
    "linux-x86_64",
    "linux-aarch64",
    "mac-x86_64",
    "mac-aarch64",
]
ARTIFACT_TARGETS = {
    "installer": TARGETS,
    "mar": TARGETS,
    "deb-ppa": ["linux-x86_64"],
    "portable": ["windows-x86_64"],
    "stub": ["windows-x86_64"],
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON field")
            result[key] = value
        return result

    data = path.read_bytes()
    if len(data) > 1024 * 1024:
        raise ValueError("oversized policy")
    return json.loads(data, object_pairs_hook=unique)


def validate_plan(plan):
    if not isinstance(plan, dict):
        raise TypeError("plan must be an object")
    if set(plan) != {
        "schemaVersion",
        "id",
        "stage",
        "origin",
        "executionKind",
        "target",
        "groups",
    }:
        raise ValueError("unknown or missing plan field")
    if type(plan["schemaVersion"]) is not int or plan["schemaVersion"] != 1:
        raise ValueError("unsupported plan schema")
    for key, value in {
        "id": "qa3-source-host-v1",
        "stage": "shadow",
        "origin": "floorp",
        "executionKind": "host-unit",
        "target": "linux-x86_64-host",
    }.items():
        if plan[key] != value:
            raise ValueError(f"invalid plan {key}")
    if (
        not isinstance(plan["groups"], list)
        or not all(isinstance(g, dict) for g in plan["groups"])
        or [g.get("id") for g in plan["groups"]] != list(GROUPS)
    ):
        raise ValueError("fixed source subset changed")
    for group in plan["groups"]:
        if set(group) != {"id", "paths", "cases"} or group["paths"] != list(
            GROUPS[group["id"]]
        ):
            raise ValueError("unregistered source command or file")
        cases = group["cases"]
        if not isinstance(cases, list) or len(cases) != CASE_COUNTS[group["id"]]:
            raise ValueError("empty or reduced source test plan")
        ids = []
        for case in cases:
            if (
                not isinstance(case, dict)
                or set(case) != {"file", "name"}
                or case["file"] not in group["paths"]
            ):
                raise ValueError("unregistered test origin")
            if not isinstance(case["name"], str) or not 0 < len(case["name"]) <= 1024:
                raise ValueError("invalid test name")
            ids.append((case["file"], case["name"]))
        if len(set(ids)) != len(ids) or {f for f, _ in ids} != set(group["paths"]):
            raise ValueError("duplicate test or missing source file")
    return plan


def validate_coverage(catalog):
    if not isinstance(catalog, dict):
        raise TypeError("coverage must be an object")
    if set(catalog) != {
        "schemaVersion",
        "stage",
        "publicationAuthorized",
        "artifacts",
        "deferred",
    }:
        raise ValueError("invalid coverage catalog")
    if type(catalog["schemaVersion"]) is not int or catalog["schemaVersion"] != 1:
        raise ValueError("invalid coverage schema")
    if catalog["stage"] != "shadow" or catalog["publicationAuthorized"] is not False:
        raise ValueError("source evidence cannot authorize publication")
    entries = catalog["artifacts"]
    if (
        not isinstance(entries, list)
        or not all(isinstance(x, dict) for x in entries)
        or [x.get("kind") for x in entries] != list(ARTIFACT_TARGETS)
    ):
        raise ValueError("missing publication artifact kind")
    for entry in entries:
        if set(entry) != {"kind", "targets", "verdict", "requires", "producer"}:
            raise ValueError("invalid artifact coverage")
        if (
            entry["targets"] != ARTIFACT_TARGETS[entry["kind"]]
            or entry["verdict"] != "UNVERIFIED"
        ):
            raise ValueError("missing target or unsupported product PASS")
        if (
            not isinstance(entry["requires"], list)
            or not entry["requires"]
            or not all(isinstance(x, str) and x for x in entry["requires"])
            or not isinstance(entry["producer"], str)
            or not entry["producer"]
        ):
            raise ValueError("artifact coverage requires an explicit proof plan")
    if (
        not isinstance(catalog["deferred"], list)
        or not catalog["deferred"]
        or not all(isinstance(x, str) and x for x in catalog["deferred"])
    ):
        raise ValueError("deferred capabilities must remain visible")
    return catalog


def validate_cases(group, actual):
    if not isinstance(actual, list) or not all(
        isinstance(c, dict)
        and set(c) == {"file", "name", "verdict"}
        and all(isinstance(v, str) for v in c.values())
        for c in actual
    ):
        raise ValueError("invalid executed case evidence")
    expected = {(c["file"], c["name"]) for c in group["cases"]}
    observed = [(c["file"], c["name"]) for c in actual]
    if len(observed) != len(set(observed)) or set(observed) != expected:
        raise ValueError("missing, duplicate, or unexpected executed test")
    if any(c["verdict"] != "PASS" for c in actual):
        raise ValueError("failed, skipped, or incomplete source test")


def audit_deno_subset(repo):
    for path in GROUPS["deno-host"]:
        text = (repo / path).read_text()
        definitions = re.findall(
            r'Deno\.test\(\s*"(?:\\.|[^"\\])*"\s*,\s*(?:async\s+)?(?:\(|function\b)',
            text,
        )
        if (
            len(definitions) != len(re.findall(r"\bDeno\.test\s*\(", text))
            or not definitions
        ):
            raise ValueError(
                "host subset requires plain tests without per-test retry/ignore options"
            )


def read_junit(path, repo, node_file=None):
    if node_file is not None and node_file not in GROUPS["node-intake"]:
        raise ValueError("unregistered single-file Node report origin")
    data = path.read_bytes()
    if (
        not data
        or len(data) > 4 * 1024 * 1024
        or re.search(rb"<!\s*(?:DOCTYPE|ENTITY)", data, re.IGNORECASE)
    ):
        raise ValueError("missing, oversized, or unsafe JUnit report")
    root = ET.fromstring(data)
    if root.tag not in {"testsuites", "testsuite"}:
        raise ValueError("unsupported JUnit document")
    for suite in root.iter():
        if suite.tag not in {"testsuites", "testsuite"}:
            continue
        for key in ("failures", "errors", "skipped", "disabled"):
            if key in suite.attrib and suite.attrib[key] != "0":
                raise ValueError("failed or skipped JUnit aggregate")
        if "tests" in suite.attrib and suite.attrib["tests"] != str(
            len(list(suite.iter("testcase")))
        ):
            raise ValueError("JUnit aggregate execution mismatch")
    cases = []
    for case in root.iter("testcase"):
        filename = case.get("file") or case.get("classname", "")
        if "file" not in case.attrib and filename == "test" and node_file is not None:
            filename = node_file
        file = Path(filename)
        if file.is_absolute():
            filename = file.relative_to(repo).as_posix()
        else:
            filename = file.as_posix().removeprefix("./")
        if case.get("status", "passed") not in {"passed", "run"} or any(
            child.tag not in {"system-out", "system-err"} for child in case
        ):
            verdict = "FAIL"
        else:
            verdict = "PASS"
        cases.append({
            "file": filename,
            "name": case.get("name", ""),
            "verdict": verdict,
        })
    return cases


def python_tests(output):
    sys.path.insert(0, str(ROOT / "tools"))
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    for path in GROUPS["python-provenance"]:
        suite.addTests(loader.loadTestsFromName(Path(path).stem))
    cases = []

    class Result(unittest.TextTestResult):
        def startTest(self, test):
            self.current = {
                "file": f"tools/{test.id().split('.')[0]}.py",
                "name": test.id(),
                "verdict": "INCOMPLETE",
            }
            cases.append(self.current)
            super().startTest(test)

        def addSuccess(self, test):
            self.current["verdict"] = "PASS"
            super().addSuccess(test)

        def addSubTest(self, test, subtest, err):
            if err is not None:
                self.current["verdict"] = "FAIL"
            super().addSubTest(test, subtest, err)

    result = unittest.TextTestRunner(verbosity=2, resultclass=Result).run(suite)
    output.write_text(json.dumps(cases, indent=2) + "\n")
    return (
        0
        if result.wasSuccessful()
        and not result.skipped
        and not result.unexpectedSuccesses
        else 1
    )


def child_environment():
    return {
        key: os.environ[key]
        for key in (
            "PATH",
            "HOME",
            "TMPDIR",
            "TEMP",
            "SYSTEMROOT",
            "DENO_DIR",
        )
        if key in os.environ
    } | {"PYTHONDONTWRITEBYTECODE": "1", "LC_ALL": "C.UTF-8"}


def execute(args, log, repo, timeout=180):
    with log.open("xb") as stream:
        child = subprocess.Popen(
            ["/usr/bin/prlimit", "--fsize=16777216:16777216", "--", *args],
            cwd=repo,
            env=child_environment(),
            stdout=stream,
            stderr=stream,
            start_new_session=True,
        )
        try:
            code = child.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGKILL)
            child.wait(timeout=10)
            raise ValueError("source command timed out")
        finally:
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    return code


def run_source(output, expected_head, local=False):
    output.mkdir(parents=True, exist_ok=False)
    plan_path = ROOT / "tools/qa3/source-plan.json"
    coverage_path = ROOT / "tools/qa3/coverage.json"
    receipt = {
        "schemaVersion": 1,
        "stage": "shadow",
        "sourceHead": None,
        "expectedSourceHead": expected_head,
        "workingTreeClean": None,
        "localExecution": local,
        "workflowRun": os.environ.get("GITHUB_RUN_ID"),
        "workflowAttempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
        "host": {"system": platform.system(), "architecture": platform.machine()},
        "tools": {"python": platform.python_version()},
        "sourceVerdict": "INCOMPLETE",
        "productVerdict": "UNVERIFIED",
        "runtimeQualification": "UNVERIFIED",
        "officialFirefoxMatch": "unverified",
        "publicationAuthorized": False,
        "steps": [],
    }
    try:
        if not re.fullmatch(r"[0-9a-f]{40}", expected_head):
            raise ValueError("expected source must be a full Git SHA")
        if sys.platform != "linux" or platform.machine() != "x86_64":
            raise ValueError(
                "this source host plan requires a disposable Linux x86_64 worker"
            )
        actual_head = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip()
        receipt["sourceHead"] = actual_head
        if actual_head != expected_head:
            raise ValueError("source checkout mismatch")
        dirty = bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--untracked-files=all"], cwd=ROOT
            )
        )
        receipt["workingTreeClean"] = not dirty
        if dirty and not local:
            raise ValueError("source checkout contains uncommitted changes")
        receipt["planSha256"] = digest(plan_path.read_bytes())
        receipt["coverageSha256"] = digest(coverage_path.read_bytes())
        plan = validate_plan(read_json(plan_path))
        audit_deno_subset(ROOT)
        receipt["coverage"] = validate_coverage(read_json(coverage_path))
        receipt["plannedCases"] = sum(len(g["cases"]) for g in plan["groups"])
        receipt["testFiles"] = {
            p: digest((ROOT / p).read_bytes())
            for paths in GROUPS.values()
            for p in paths
        }
        receipt["denoLockSha256"] = digest((ROOT / "deno.lock").read_bytes())
        for tool in ("deno", "node"):
            log = output / f"{tool}-version.log"
            try:
                code = execute([tool, "--version"], log, ROOT, timeout=10)
                receipt["tools"][tool] = {
                    "exitCode": code,
                    "version": log.read_text()[:4096].strip(),
                }
            except (ValueError, OSError) as error:
                receipt["tools"][tool] = {"verdict": "UNVERIFIED", "error": str(error)}
        for group in plan["groups"]:
            record = {"id": group["id"], "verdict": "INCOMPLETE", "cases": []}
            receipt["steps"].append(record)
            report = output / f"{group['id']}.xml"
            args = {
                "deno-host": [
                    "deno",
                    "test",
                    "--frozen-lockfile",
                    "--node-modules-dir=auto",
                    "--no-prompt",
                    "--allow-read",
                    "--allow-write",
                    "--retry=0",
                    "--junit-path",
                    str(report),
                    *GROUPS["deno-host"],
                ],
                "node-intake": [
                    "node",
                    "--test",
                    "--test-reporter=junit",
                    *GROUPS["node-intake"],
                ],
                "python-provenance": [
                    sys.executable,
                    "-B",
                    str(Path(__file__).resolve()),
                    "python-tests",
                    str(report),
                ],
            }[group["id"]]
            log = output / f"{group['id']}.log"
            try:
                code = execute(args, log, ROOT)
                record["exitCode"] = code
                if group["id"] == "node-intake":
                    record["cases"] = read_junit(log, ROOT, GROUPS["node-intake"][0])
                elif group["id"] == "python-provenance":
                    record["cases"] = read_json(report)
                else:
                    record["cases"] = read_junit(report, ROOT)
                validate_cases(group, record["cases"])
                if code != 0:
                    raise ValueError("source command failed")
                record["verdict"] = "PASS"
            except (ValueError, TypeError, OSError, ET.ParseError) as error:
                record["error"] = str(error)
                record["verdict"] = "FAIL"
        receipt["sourceVerdict"] = (
            "PASS" if all(s["verdict"] == "PASS" for s in receipt["steps"]) else "FAIL"
        )
    except (ValueError, TypeError, OSError, subprocess.SubprocessError) as error:
        receipt["error"] = str(error)
        receipt["sourceVerdict"] = "FAIL"
    finally:
        receipt["executedCases"] = sum(len(s["cases"]) for s in receipt["steps"])
        receipt["runnerSha256"] = digest(Path(__file__).read_bytes())
        (output / "source-receipt.json").write_text(
            json.dumps(receipt, indent=2) + "\n"
        )
        summary = {
            "sourceHead": receipt["sourceHead"],
            "sourceVerdict": receipt["sourceVerdict"],
            "executedCases": receipt["executedCases"],
            "publicationAuthorized": False,
            "steps": [],
        }
        if "error" in receipt:
            summary["error"] = receipt["error"]
        for step in receipt["steps"]:
            item = {key: value for key, value in step.items() if key != "cases"}
            item["executedCases"] = len(step["cases"])
            if step["verdict"] != "PASS":
                log = output / f"{step['id']}.log"
                if log.is_file():
                    with log.open("rb") as stream:
                        stream.seek(max(0, log.stat().st_size - 4096))
                        item["logTail"] = stream.read(4096).decode(
                            "utf-8", errors="replace"
                        )
            summary["steps"].append(item)
        print(json.dumps(summary))
    return 0 if receipt["sourceVerdict"] == "PASS" else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["run", "python-tests"])
    parser.add_argument("output", type=Path)
    parser.add_argument("--expected-head")
    parser.add_argument("--local", action="store_true")
    args = parser.parse_args()
    if args.command == "python-tests":
        return python_tests(args.output)
    return run_source(args.output.resolve(), args.expected_head or "", args.local)


if __name__ == "__main__":
    sys.exit(main())
