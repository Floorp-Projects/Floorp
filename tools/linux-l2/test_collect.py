# SPDX-License-Identifier: MPL-2.0
"""Host-only contract tests; all product/provider inputs below are synthetic."""
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
import zipfile

import collect


def fixture():
    source, probe = "a" * 40, "b" * 40
    producer = {"id": 11, "head_sha": source, "run_attempt": 1, "event": "workflow_dispatch",
                "path": ".github/workflows/package_and_publish_release.yml",
                "repository": {"full_name": "Floorp-Projects/Floorp"}}
    run = {**producer, "id": 22, "head_sha": probe,
           "path": ".github/workflows/validate-final-package-qa.yml"}
    package = {"id": 31, "name": "noraneko-linux-x86_64-installer", "digest": "sha256:" + "c" * 64}
    native = {"id": 32, "name": "floorp-native-verification-linux-x86_64-v2", "digest": "sha256:" + "d" * 64}
    inp = {"runId": 11, "sourceAttempt": 1, "floorpHead": source, "probeHead": probe,
           "unsigned": False, "platform": "linux", "arch": "x86_64", "package": package,
           "record": native, "floorpVersion": "12.20.2", "firefoxVersion": "128.0",
           "runtimeBuildId": "20261012000000"}
    record = {"schema_version": 2, "verification": {"status": "verified"}, "build_id2": source,
              "floorp": {"workflow_run_id": 11, "head_sha": source, "repository": "Floorp-Projects/Floorp"},
              "target_key": "linux", "native_arch": "x86_64", "firefox_version": "128.0",
              "runtime": {"expected_build_id": "20261012000000"},
              "app_build_id": "20261012000000", "platform_build_id": "20261012000000",
              "floorp_package": {"artifact_id": 31, "artifact_name": package["name"],
                                "artifact_digest": package["digest"], "unsigned": False}}
    identity = {"version": "128.0", "appBuildID": "20261012000000", "platformBuildID": "20261012000000",
                "buildID2": source, "startupMode": "production", "httpLoaderAllowed": False,
                "sandboxLevel": 6, "version2": "12.20.2", "floorpVersion": "12.20.2"}
    report = {"status": "passed", "nativeRecord": record, "hostProcessIdentity": {"pid": 42, "verified": True},
              "ownedHostPid": 42, "hostExitCode": -15, "cleanupRequestedSignal": 15,
              "identity": identity, "checks": [{"key": "synthetic-host-contract"}],
              "httpFixtureError": None,
              "redirectChecks": [{"caseId": "web-panel-ua.redirect.mobile-" + mode, "status": "passed"}
                                 for mode in ("on", "off")],
              "browserRegression": {"status": "passed", "identity": identity,
                                    "checks": [{"name": "synthetic-host-contract"}]}}
    steps = [{"name": name, "status": "completed", "conclusion": "success"} for name in (
        "Authenticate the normal package run and exact artifacts",
        "Validate the native record and extract the exact package",
        "Set up the Linux display and extract the package", *collect.PROBES.values())]
    job = {"id": 44, "run_id": 22, "run_attempt": 1, "name": collect.JOB, "status": "completed",
           "conclusion": "success", "labels": [collect.RUNNER], "runner_name": "synthetic-fixture",
           "steps": steps}
    context = {"run": run, "selection": {"executionRunId": 22, "executionAttempt": 1,
        "packageRunId": 11, "floorpHead": source, "probeHead": probe}, "originalJobs": [job],
        "packageRun": producer, "packageJobs": [{"id": 45, "run_id": 11, "run_attempt": 1,
            "name": "Native / Verify linux/x86_64 package", "status": "completed", "conclusion": "success"}],
        "packageArtifacts": [{**value, "expired": False, "workflow_run": {"id": 11, "head_sha": source}}
                             for value in (package, native)], "collectionErrors": []}
    retained = {"input.json": inp, "native-verification.json": record,
                **{f"{case}/run-first/report.json": copy.deepcopy(report) for case in collect.PROBES}}
    inventory = {f"{case}/run-first/browser.log": {"bytes": 10, "sha256": "e" * 64} for case in collect.PROBES}
    return context, retained, inventory


class ObservationTests(unittest.TestCase):
    def test_complete_original_scope_pass_never_grants_publication_or_global_history(self):
        result = collect.collect(*fixture())
        self.assertEqual(result["status"], "PASS_SCOPE_ONLY")
        self.assertEqual((result["executedCases"], result["passedCases"]), (2, 2))
        self.assertIs(result["publicationAuthorized"], False)
        for key in ("globalFirstEvaluation", "agentQualification", "secretIsolationQualification",
                    "descendantAndPortCleanup"):
            self.assertEqual(result[key], "UNVERIFIED")

    def test_missing_reports_logs_and_duplicates_do_not_pass(self):
        for mutation in ("missing-report", "missing-log", "duplicate"):
            with self.subTest(mutation=mutation):
                context, reports, inventory = fixture()
                path = "web-panel-ua/run-first/report.json"
                if mutation == "missing-report":
                    del reports[path]
                elif mutation == "missing-log":
                    inventory.clear()
                else:
                    reports["web-panel-ua/run-second/report.json"] = copy.deepcopy(reports[path])
                self.assertNotEqual(collect.collect(context, reports, inventory)["status"], "PASS_SCOPE_ONLY")

    def test_original_failure_survives_clean_reports_and_later_retry(self):
        context, reports, inventory = fixture()
        job = context["originalJobs"][0]
        job["conclusion"] = "failure"
        job["steps"][3]["conclusion"] = "failure"
        for attempt in (1, 2):
            context["run"]["run_attempt"] = context["selection"]["executionAttempt"] = attempt
            result = collect.collect(context, reports, inventory)
            self.assertEqual(result["status"], "FAIL")
            self.assertEqual(result["originalFailureSteps"][0]["name"], collect.PROBES["web-panel-ua"])
            self.assertIs(result["publicationAuthorized"], False)

    def test_missing_archive_does_not_hide_original_provider_failure(self):
        context, reports, inventory = fixture()
        context["originalJobs"][0]["conclusion"] = "failure"
        context["collectionErrors"] = ["raw upload missing"]
        result = collect.collect(context, {}, {})
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["originalJob"]["id"], 44)
        self.assertEqual(result["executedCases"], 0)

    def test_skipped_missing_or_incomplete_provider_steps_never_receive_credit(self):
        for conclusion in ("skipped", None, "cancelled"):
            context, reports, inventory = fixture()
            context["originalJobs"][0]["steps"][3]["conclusion"] = conclusion
            result = collect.collect(context, reports, inventory)
            self.assertNotEqual(result["status"], "PASS_SCOPE_ONLY")
            self.assertEqual(result["cases"][0]["status"], "NOT_EXECUTED")

    def test_foreign_source_job_attempt_and_artifact_bindings_are_rejected(self):
        mutations = (
            lambda c, r: c["run"].update(run_attempt=2),
            lambda c, r: c["originalJobs"][0].update(run_id=99),
            lambda c, r: c["originalJobs"][0].update(run_attempt=2),
            lambda c, r: c["originalJobs"][0].update(runner_name=None),
            lambda c, r: c["originalJobs"][0].update(labels=["ubuntu-24.04"]),
            lambda c, r: c["packageRun"].update(run_attempt=2),
            lambda c, r: c["packageJobs"][0].update(run_attempt=2),
            lambda c, r: c["packageArtifacts"][0].update(id=99),
            lambda c, r: c["packageArtifacts"][0].update(expired=True),
            lambda c, r: c["packageArtifacts"][0]["workflow_run"].update(head_sha="f" * 40),
            lambda c, r: r["input.json"].update(unsigned=True),
            lambda c, r: r["input.json"].update(sourceAttempt=2),
            lambda c, r: r["input.json"].update(probeHead="f" * 40),
            lambda c, r: r["native-verification.json"]["floorp_package"].update(artifact_id=99),
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                context, reports, inventory = fixture()
                mutation(context, reports)
                self.assertNotEqual(collect.collect(context, reports, inventory)["status"], "PASS_SCOPE_ONLY")

    def test_raw_pass_cannot_hide_forced_cleanup_loader_or_identity_failure(self):
        for change in ({"cleanupRequestedSignal": 9, "hostExitCode": -9},
                       {"hostCleanupError": "failed"}, {"ownedHostPid": 99},
                       {"hostExitCode": None}, {"cleanupRequestedSignal": None, "hostExitCode": -15}):
            context, reports, inventory = fixture()
            reports["web-panel-ua/run-first/report.json"].update(change)
            self.assertEqual(collect.collect(context, reports, inventory)["status"], "FAIL")
        for field, value in (("httpLoaderAllowed", True), ("sandboxLevel", 0),
                             ("buildID2", "foreign"), ("version2", "foreign")):
            context, reports, inventory = fixture()
            reports["web-panel-ua/run-first/report.json"]["identity"][field] = value
            self.assertEqual(collect.collect(context, reports, inventory)["status"], "FAIL")

    def test_both_original_redirects_and_context_checks_are_required(self):
        context, reports, inventory = fixture()
        reports["web-panel-ua/run-first/report.json"]["redirectChecks"].pop()
        reports["fluent-flasco/run-first/report.json"]["browserRegression"]["checks"] = []
        result = collect.collect(context, reports, inventory)
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["passedCases"], 0)

    def test_duplicate_nonfinite_or_oversize_json_rejected(self):
        for value in (b'{"a":1,"a":2}', b'{"a":NaN}', b'{"a":1e400}', b" " * (2 * 1024**2 + 1)):
            with self.assertRaises(ValueError):
                collect.json_bytes(value)

    def test_malformed_steps_and_typed_attempts_remain_blocked_observations(self):
        context, reports, inventory = fixture()
        context["originalJobs"][0]["steps"] = None
        result = collect.collect(context, reports, inventory)
        self.assertEqual(result["status"], "BLOCK")
        self.assertEqual(result["originalJob"]["id"], 44)
        for value in (True, 1.0):
            context, reports, inventory = fixture()
            context["run"]["run_attempt"] = value
            self.assertEqual(collect.collect(context, reports, inventory)["status"], "BLOCK")

    def test_cli_uses_original_archive_digest_and_creation_only_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            context, retained, inventory = fixture()
            archive = root / "raw.zip"
            with zipfile.ZipFile(archive, "w") as stream:
                for name, value in retained.items():
                    stream.writestr(name, json.dumps(value))
                for name in inventory:
                    stream.writestr(name, b"synthetic original browser log\n")
            context["rawArtifactIdExpected"] = 55
            context["rawArtifact"] = {"id": 55, "name": "final-package-qa-linux-x86_64-22-1",
                "digest": "sha256:" + hashlib.sha256(archive.read_bytes()).hexdigest(),
                "expired": False, "workflow_run": {"id": 22, "head_sha": context["run"]["head_sha"]}}
            ctx = root / "context.json"
            ctx.write_text(json.dumps(context))
            command = [sys.executable, "-B", collect.__file__, "--context", str(ctx),
                       "--archive", str(archive), "--output", str(root / "out")]
            completed = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertEqual(completed.returncode, 0, completed.stderr + completed.stdout)
            before = (root / "out/observation.json").read_bytes()
            self.assertNotEqual(subprocess.run(command, capture_output=True, timeout=10).returncode, 0)
            self.assertEqual((root / "out/observation.json").read_bytes(), before)
            context["rawArtifact"]["digest"] = "sha256:" + "0" * 64
            ctx.write_text(json.dumps(context))
            command[-1] = str(root / "wrong")
            failed = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertEqual(failed.returncode, 1)
            result = json.loads((root / "wrong/observation.json").read_text())
            self.assertEqual(result["status"], "BLOCK")

    def test_archive_traversal_duplicate_and_symlink_rejected_without_extraction(self):
        for mode in ("traversal", "duplicate", "symlink"):
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                archive = root / "raw.zip"
                with zipfile.ZipFile(archive, "w") as stream:
                    if mode == "symlink":
                        entry = zipfile.ZipInfo("input.json")
                        entry.external_attr = 0o120777 << 16
                        stream.writestr(entry, "/outside")
                    else:
                        stream.writestr("../outside" if mode == "traversal" else "same", "{}")
                        if mode == "duplicate":
                            stream.writestr("same", "{}")
                (root / "out").mkdir()
                digest = "sha256:" + hashlib.sha256(archive.read_bytes()).hexdigest()
                with self.assertRaises(ValueError):
                    collect.read_archive(archive, root / "out", {"id": 55, "digest": digest})
                self.assertFalse((root / "outside").exists())

    def test_workflow_keeps_default_targets_and_explicit_paid_selection(self):
        text = (collect.ROOT / ".github/workflows/validate-final-package-qa.yml").read_text()
        matches = re.search(r"matrix: \$\{\{ fromJSON\(inputs.linux_l2_blacksmith && '([^']+)' \|\| '([^']+)'\) \}\}", text)
        self.assertIsNotNone(matches)
        selected, default = (json.loads(value)["include"] for value in matches.groups())
        self.assertEqual([(row["platform"], row["arch"], row["runner"]) for row in selected],
                         [("linux", "x86_64", collect.RUNNER)])
        self.assertEqual({(row["platform"], row["arch"]) for row in default},
                         {("linux", "x86_64"), ("mac", "x86_64"), ("mac", "aarch64")})
        self.assertNotIn("pull_request:", text)
        self.assertIn("needs.l2-selection.result == 'success'", text)
        self.assertIn("Original package attempt 1 required", text)
        self.assertIn("if-no-files-found: error", text)
        self.assertNotIn("continue-on-error", text)


if __name__ == "__main__":
    unittest.main()
