# SPDX-License-Identifier: MPL-2.0
"""File/CLI integration with the pinned real Mozilla raw-log verifier; no native browser."""
import copy
import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("debug_collect", HERE / "collect.py")
collector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collector)
POLICY = json.loads((HERE / "runtime-recipe.json").read_text())


def encoded(value):
    return (json.dumps(value, separators=(",", ":")) + "\n").encode()


def identity(data):
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


class Export:
    def __init__(self, root):
        self.root = root
        self.members = {}
        self.runtime = POLICY["runtimeHead"]
        self.context = {"schema": "qa3-hosted-context-v1", "profile": "github-hosted-debug-to-blacksmith-consumer",
            "executionRepository": "Floorp-Projects/Floorp-Runtime", "controllerHead": self.runtime,
            "runtimeRecipe": self.runtime, "run": 12345, "attempt": 1, "buildID": "20261016000000",
            "publicationAuthorized": False}
        config = {"substs": {"ENABLE_TESTS": True, "MOZ_DEBUG": True, "COMPILE_ENVIRONMENT": True,
            "TARGET_CPU": "x86_64", "OS_TARGET": "Linux"}}
        self.producer = {"schema": "qa3-hosted-debug-producer-v1", "status": "PACKAGED_NOT_CONSUMED",
            "producer": "existing-common-build-github-hosted", "context": self.context,
            "bootSha256": "a" * 64, "consumerVerdict": "UNVERIFIED", "publicationAuthorized": False,
            "effectiveConfig": config, "effectiveConfigAfterTests": copy.deepcopy(config),
            "primarySha256": "d" * 64, "supportSha256": "e" * 64,
            "planSha256": POLICY["files"][".github/qa/hosted-linux-debug-plan.json"]["sha256"]}
        self.consumer = {"schema": "qa3-hosted-cold-consumer-v1", "status": "PASS", "context": self.context,
            "producerBootSha256": "a" * 64, "consumerBootSha256": "b" * 64, "commands": [], "cases": [],
            "nativeProductQualification": "UNVERIFIED", "publicationAuthorized": False}
        for phase, canary in zip(POLICY["phases"], (False, True)):
            test = "xpcom/tests/unit/" + ("test_qa3_assertion.js" if canary else "test_nsIMutableArray.js")
            self.members["export/logs/" + phase + ".mozlog"] = self.events(test, canary)
            support, app = "/original/state/support", "/original/state/primary/floorp"
            command = {"name": phase, "argv": ["/usr/bin/xvfb-run", "-a", "/original/state/venv/bin/python", "-s",
                "/original/recipe/.github/workflows/scripts/qa3_xpcshell_entry.py", "--xpcshell", support + "/bin/xpcshell",
                "--app-path", app, "--sequential", "--verbose", "--no-logfiles", "--manifest",
                support + "/xpcshell/tests/xpcom/tests/unit/xpcshell.toml", "--xre-path", app,
                "--utility-path", support + "/bin", "--testing-modules-dir", support + "/modules",
                "--log-raw", "/original/state/output/logs/" + phase + ".mozlog", support + "/xpcshell/tests/" + test],
                "exit": 1 if canary else 0, "stop": None, "ownedCleanupComplete": True, "stdoutIsData": False,
                "cancelled": False, "error": None, "cleanupErrors": [],
                "stdoutSha256": identity(b"")["sha256"], "stderrSha256": identity(b"")["sha256"]}
            for stream in ("stdout", "stderr"):
                self.members[f"export/logs/{phase}.{stream}"] = b""
            verdict = {"id": test, "suite": "xpcshell", "assertions": 1, "rawEvents": 5,
                "verdict": "EXPECTED_REJECTION" if canary else "PASS",
                "canaryPhase": "test_status" if canary else None,
                "canaryReason": "QA3_EXPECTED_ASSERTION" if canary else None}
            self.consumer["commands"].append(command)
            self.consumer["cases"].append({"name": phase, "suite": "xpcshell", "canary": canary,
                "process": command, "mozlog": verdict})
        self.final = {"schema": "qa3-hosted-root-supervision-v1", "context": self.context,
            "status": "PASS", "reason": None, "scopeEmpty": True, "serviceExit": 0, "publicationAuthorized": False}
        self.snapshot = {"run": {"id": 12345, "repository": {"full_name": "Floorp-Projects/Floorp-Runtime"},
            "head_sha": self.runtime, "run_attempt": 1, "path": ".github/workflows/daily-build.yml",
            "status": "completed", "conclusion": "success"},
            "job": {"id": 23456, "run_id": 12345, "head_sha": self.runtime, "run_attempt": 1, "name": "Linux x64 / Cold Linux x64 Debug harness",
                "status": "completed", "conclusion": "success", "steps": [{"name": name, "status": "completed", "conclusion": "success"}
                for name in ("Acquire original same-run producer artifact", "Execute dedicated cold consumer", "Collect original bounded evidence")]}}
        self.product = {"floorp": {"head_sha": "f" * 40, "workflow_run_id": 56789},
            "runtime": {"head_sha": self.runtime, "workflow_run_id": 45678}, "app_build_id": "20261014000000"}

    @staticmethod
    def events(test, canary):
        return b"".join(encoded(e) for e in [
            {"action": "suite_start", "tests": [test]}, {"action": "test_start", "test": test},
            {"action": "test_status", "test": test, "subtest": "run_test", "status": "FAIL" if canary else "PASS",
             "expected": "PASS", "message": "[run_test : 1] QA3_EXPECTED_ASSERTION - false == true" if canary else "ok"},
            {"action": "test_end", "test": test, "status": "FAIL" if canary else "PASS"}, {"action": "suite_end"}])

    def save(self):
        for item, command in zip(self.consumer["cases"], self.consumer["commands"]):
            phase = item["name"]
            raw_identity = identity(self.members["export/logs/" + phase + ".mozlog"])
            item["rawLog"] = {"path": "logs/" + phase + ".mozlog", **raw_identity}
            command["watchedLogs"] = {phase + ".mozlog": raw_identity}
            self.members["export/logs/" + phase + ".process.json"] = encoded(command)
        self.members["export/consumer.json"] = encoded(self.consumer)
        rows = [{"path": n.removeprefix("export/"), **identity(b)} for n, b in sorted(self.members.items())
                if n.startswith("export/") and n != "export/export.json"]
        self.final["export"] = rows
        self.members["export/export.json"] = encoded({"schema": "qa3-hosted-export-v1", "files": rows})
        self.members["evidence/final.json"] = encoded(self.final)
        self.members["collection.json"] = encoded({"schema": "qa3-hosted-collection-v1", "context": self.context,
            "acquireOutcome": "success", "launchOutcome": "success", "publicationAuthorized": False})
        archive = self.root / "consumer.zip"
        with zipfile.ZipFile(archive, "w") as output:
            for name, data in self.members.items():
                output.writestr(name, data)
        data = archive.read_bytes()
        self.snapshot["artifact"] = {"id": 34567, "name": "qa3-consumer-evidence-12345-1", "expired": False,
            "workflow_run": {"id": 12345, "head_sha": self.runtime}, "size_in_bytes": len(data),
            "digest": "sha256:" + identity(data)["sha256"]}
        self.snapshot["producerReceiptSha256"] = identity(encoded(self.producer))["sha256"]
        for name, value in (("provider", self.snapshot), ("producer", self.producer), ("product", self.product)):
            (self.root / (name + ".json")).write_bytes(encoded(value))

    def run(self, output="result"):
        self.save()
        reply = subprocess.run([sys.executable, "-I", "-B", str(HERE / "collect.py"),
            "--provider", str(self.root / "provider.json"), "--producer", str(self.root / "producer.json"),
            "--archive", str(self.root / "consumer.zip"), "--runtime-git", os.environ["QA3_RUNTIME_RECIPE"],
            "--product-record", str(self.root / "product.json"), "--output", str(self.root / output)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90)
        path = self.root / output / "observation.json"
        return reply, json.loads(path.read_text()) if path.exists() else None


class CollectorIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.environ.get("QA3_RUNTIME_RECIPE"):
            raise RuntimeError("QA3_RUNTIME_RECIPE must point to a checkout containing the pinned public Git objects")

    def test_cli_replays_real_pinned_mozilla_parser_and_keeps_subjects_separate(self):
        with tempfile.TemporaryDirectory() as temp:
            f = Export(Path(temp))
            reply, result = f.run()
            self.assertEqual(reply.returncode, 0, reply.stderr.decode())
            self.assertEqual(result["status"], "PASS_DEBUG_EVIDENCE_ONLY")
            self.assertEqual(len(result["cases"]), 2)
            self.assertNotEqual(result["debugSubject"]["buildID"], result["productSubject"]["nativeBuildID"])
            self.assertEqual(result["productApplicability"], "UNVERIFIED")
            self.assertFalse(result["publicationAuthorized"])
            self.assertFalse(result["nativeLaunchedByCollector"])
            before = (f.root / "result/observation.json").read_bytes()
            retry, _ = f.run()
            self.assertNotEqual(retry.returncode, 0)
            self.assertEqual((f.root / "result/observation.json").read_bytes(), before)

    def test_rehashed_incomplete_log_cannot_be_recovered_by_a_clean_receipt(self):
        with tempfile.TemporaryDirectory() as temp:
            f = Export(Path(temp))
            name = "export/logs/native-1-required.mozlog"
            f.members[name] = b"\n".join(f.members[name].splitlines()[:-1]) + b"\n"
            reply, result = f.run()
            self.assertNotEqual(reply.returncode, 0)
            self.assertEqual(result["status"], "FAIL")
            self.assertTrue((f.root / "result/replay.log").is_file())

    def test_original_failure_and_retry_metadata_cannot_be_hidden(self):
        for mode in ("job-failure", "failed-step", "other-command-failure", "retry", "skip", "malformed-steps"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temp:
                f = Export(Path(temp))
                if mode == "job-failure":
                    f.snapshot["job"]["conclusion"] = "failure"
                elif mode == "failed-step":
                    f.snapshot["job"]["steps"][1]["conclusion"] = "failure"
                elif mode == "other-command-failure":
                    f.consumer["commands"].append({"name": "native-0-required", "exit": 1,
                        "ownedCleanupComplete": True})
                elif mode == "retry":
                    f.snapshot["run"]["run_attempt"] = 2
                elif mode == "malformed-steps":
                    f.snapshot["job"]["steps"] = [None]
                else:
                    f.snapshot["job"]["steps"][1]["conclusion"] = "skipped"
                reply, result = f.run()
                self.assertNotEqual(reply.returncode, 0)
                self.assertEqual(result["status"], "FAIL" if "fail" in mode or mode == "failed-step" else "BLOCK")
                self.assertFalse((f.root / "result/runtime-recipe").exists())

    def test_original_argv_and_phase_membership_are_checked_before_raw_replay(self):
        for mode in ("argv", "missing-phase"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temp:
                f = Export(Path(temp))
                if mode == "argv":
                    f.consumer["commands"][0]["argv"][-1] = "/foreign/test.js"
                else:
                    f.consumer["cases"].pop()
                reply, result = f.run()
                self.assertNotEqual(reply.returncode, 0)
                self.assertEqual(result["status"], "BLOCK")

    def test_another_runtime_revision_and_pgo_configuration_are_not_debug_evidence(self):
        for mode in ("source", "pgo", "subject-alias"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temp:
                f = Export(Path(temp))
                if mode == "source":
                    f.snapshot["run"]["head_sha"] = "f" * 40
                elif mode == "pgo":
                    f.producer["effectiveConfig"]["substs"]["MOZ_PROFILE_USE"] = True
                else:
                    f.producer["context"] = copy.deepcopy(f.context)
                    f.producer["context"]["attempt"] = True
                reply, result = f.run()
                self.assertNotEqual(reply.returncode, 0)
                self.assertEqual(result["status"], "BLOCK")

    def test_retained_worker_failure_is_not_erased_by_other_clean_records(self):
        with tempfile.TemporaryDirectory() as temp:
            f = Export(Path(temp))
            f.members["export/worker-failure.json"] = encoded({"status": "FAIL"})
            reply, result = f.run()
            self.assertNotEqual(reply.returncode, 0)
            self.assertEqual(result["status"], "FAIL")
            self.assertFalse((f.root / "result/runtime-recipe").exists())

    def test_recorded_verdict_numeric_alias_is_rejected_by_the_real_parser_replay(self):
        with tempfile.TemporaryDirectory() as temp:
            f = Export(Path(temp))
            f.consumer["cases"][0]["mozlog"]["assertions"] = True
            reply, result = f.run()
            self.assertNotEqual(reply.returncode, 0)
            self.assertEqual(result["status"], "FAIL")
            self.assertTrue((f.root / "result/replay.log").is_file())


if __name__ == "__main__":
    unittest.main()
