# SPDX-License-Identifier: MPL-2.0
import copy
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import source


class RequiredSourceTests(unittest.TestCase):
    def receipt(self):
        root = source.ROOT
        plan = source.read_json(root / "tools/qa3/source-plan.json")
        hashes = {
            "planSha256": "tools/qa3/source-plan.json",
            "coverageSha256": "tools/qa3/coverage.json",
            "denoLockSha256": "deno.lock",
            "denoConfigSha256": "deno.json",
            "denoHostImportMapSha256": "tools/qa3/deno-host-imports.json",
            "denoHostLockSha256": "tools/qa3/deno-host.lock",
            "runnerSha256": "tools/qa3/source.py",
        }
        return {
            "schemaVersion": 1, "stage": "required-source",
            "sourceHead": "a" * 40, "expectedSourceHead": "a" * 40,
            "workingTreeClean": True, "localExecution": False,
            "workflowRepositoryId": "437505831", "workflowRun": "123",
            "workflowAttempt": "1", "host": {"system": "Linux", "architecture": "x86_64"},
            "sourceVerdict": "PASS", "productVerdict": "UNVERIFIED",
            "runtimeQualification": "UNVERIFIED", "officialFirefoxMatch": "unverified",
            "publicationAuthorized": False, "plannedCases": 106, "executedCases": 106,
            **{k: source.digest((root / p).read_bytes()) for k, p in hashes.items()},
            "coverage": source.read_json(root / "tools/qa3/coverage.json"),
            "testFiles": {p: source.digest((root / p).read_bytes()) for paths in source.GROUPS.values() for p in paths},
            "tools": {"python": "3.12.3", "node": {"exitCode": 0, "version": "v22.20.0"},
                      "deno": {"exitCode": 0, "version": "deno 2.9.6 (stable, release, x86_64-unknown-linux-gnu)\nv8 14\ntypescript 5"}},
            "steps": [{"id": g["id"], "verdict": "PASS", "exitCode": 0,
                       "cases": [{**case, "verdict": "PASS"} for case in g["cases"]]} for g in plan["groups"]],
        }

    def test_only_complete_same_run_original_source_receipt_is_eligible(self):
        self.assertEqual(source.validate_required_receipt(self.receipt(), "a" * 40, "123")["verdict"], "PASS_SOURCE_ONLY")
        for key, value in (
            ("stage", "shadow"), ("sourceHead", "b" * 40), ("expectedSourceHead", "b" * 40),
            ("workflowRun", "124"), ("workflowAttempt", "2"), ("workflowRepositoryId", "1"),
            ("workingTreeClean", False), ("localExecution", True), ("executedCases", 105),
            ("productVerdict", "PASS"), ("publicationAuthorized", True), ("schemaVersion", True),
            ("runnerSha256", "0" * 64), ("denoHostLockSha256", "0" * 64),
        ):
            with self.subTest(key=key), self.assertRaises(ValueError):
                source.validate_required_receipt({**self.receipt(), key: value}, "a" * 40, "123")
        for receipt in ({}, {**self.receipt(), "error": "original failure"}):
            with self.assertRaises(ValueError):
                source.validate_required_receipt(receipt, "a" * 40, "123")

    def test_recovered_skip_missing_duplicate_and_wrong_toolchain_cannot_qualify(self):
        changes = []
        for verdict in ("SKIP", "FAIL", "RECOVERED", "NOT_RUN"):
            item = self.receipt(); item["steps"][0]["cases"][0]["verdict"] = verdict
            changes.append(item)
        item = self.receipt(); item["steps"][0]["cases"].pop(); changes.append(item)
        item = self.receipt(); item["steps"][0]["cases"].append(copy.deepcopy(item["steps"][0]["cases"][0])); changes.append(item)
        item = self.receipt(); item["steps"][0]["exitCode"] = 1; changes.append(item)
        item = self.receipt(); item["tools"]["deno"]["version"] = "deno 2.9.5\nv8 14"; changes.append(item)
        item = self.receipt(); item["tools"]["node"]["version"] = "v24.0.0"; changes.append(item)
        item = self.receipt(); item["testFiles"][next(iter(item["testFiles"]))] = "0" * 64; changes.append(item)
        for item in changes:
            with self.subTest(item=item.get("tools")), self.assertRaises(ValueError):
                source.validate_required_receipt(item, "a" * 40, "123")

    def test_required_mode_rejects_rerun_local_and_foreign_before_execution(self):
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source.ROOT, text=True).strip()
        for env, local in (({"GITHUB_RUN_ATTEMPT": "2"}, False), ({"GITHUB_REPOSITORY_ID": "1"}, False), ({}, True)):
            context = {"GITHUB_REPOSITORY_ID": "437505831", "GITHUB_RUN_ID": "123", "GITHUB_RUN_ATTEMPT": "1", **env}
            with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, context), patch.object(source, "execute") as execute:
                output = Path(directory) / "source"
                self.assertEqual(source.run_source(output, head, local=local, required=True), 1)
                execute.assert_not_called()
                receipt = source.read_json(output / "source-receipt.json")
                self.assertEqual(receipt["sourceVerdict"], "FAIL")
                self.assertEqual(receipt["executedCases"], 0)


if __name__ == "__main__":
    unittest.main()
