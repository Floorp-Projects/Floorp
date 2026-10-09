# SPDX-License-Identifier: MPL-2.0

import copy
import os
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

import source


class SourceContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.plan = source.read_json(source.ROOT / "tools/qa3/source-plan.json")
        self.coverage = source.read_json(source.ROOT / "tools/qa3/coverage.json")

    def test_fixed_plan_has_all_106_unique_host_cases(self):
        source.validate_plan(self.plan)
        self.assertEqual(sum(len(g["cases"]) for g in self.plan["groups"]), 106)
        source.audit_deno_subset(source.ROOT)

    def test_missing_empty_reordered_and_duplicate_groups_are_rejected(self):
        changes = [
            [],
            self.plan["groups"][:-1],
            self.plan["groups"][::-1],
            self.plan["groups"] * 2,
        ]
        for groups in changes:
            with self.subTest(groups=groups), self.assertRaises(ValueError):
                source.validate_plan({**self.plan, "groups": groups})

    def test_browser_tasks_arbitrary_arguments_and_foreign_origin_are_rejected(self):
        for key, value in (
            ("origin", "mozilla"),
            ("executionKind", "browser"),
            ("stage", "required"),
            ("target", "mac-aarch64"),
        ):
            with self.subTest(key=key), self.assertRaises(ValueError):
                source.validate_plan({**self.plan, key: value})
        for field, value in (
            ("paths", ["tools/src/colocated_test_runner.ts"]),
            ("argv", ["deno", "task", "test:integration"]),
        ):
            changed = copy.deepcopy(self.plan)
            changed["groups"][0][field] = value
            with self.assertRaises(ValueError):
                source.validate_plan(changed)

    def test_reduced_or_duplicated_test_ids_are_rejected(self):
        for transform in (lambda c: c[:-1], lambda c: c[:1] * len(c), lambda c: []):
            changed = copy.deepcopy(self.plan)
            changed["groups"][0]["cases"] = transform(changed["groups"][0]["cases"])
            with self.assertRaises(ValueError):
                source.validate_plan(changed)

    def test_unexecuted_duplicate_unknown_failed_and_skipped_cases_cannot_pass(self):
        group = self.plan["groups"][0]
        actual = [{**c, "verdict": "PASS"} for c in group["cases"]]
        source.validate_cases(group, actual)
        changes = [
            actual[:-1],
            actual + actual[:1],
            [{**actual[0], "name": "unknown"}, *actual[1:]],
        ]
        for verdict in ("FAIL", "SKIP", "INCOMPLETE", "RECOVERED"):
            changes.append([{**actual[0], "verdict": verdict}, *actual[1:]])
        for changed in changes:
            with self.subTest(changed=changed[0]), self.assertRaises(ValueError):
                source.validate_cases(group, changed)

    def test_junit_checks_case_identity_and_completion(self):
        report = self.root / "report.xml"
        report.write_text(
            '<testsuites tests="1" failures="0"><testsuite><testcase classname="./tools/src/utils.test.ts" name="one"/></testsuite><!-- pass 1 --></testsuites>'
        )
        self.assertEqual(
            source.read_junit(report, self.root),
            [{"file": "tools/src/utils.test.ts", "name": "one", "verdict": "PASS"}],
        )
        report.write_text(
            '<testsuite><testcase file="/foreign/source.ts" name="one"/></testsuite>'
        )
        with self.assertRaises(ValueError):
            source.read_junit(report, self.root)

    def test_junit_rejects_entities_skip_failure_and_aggregate_contradictions(self):
        report = self.root / "report.xml"
        for text in (
            '<!DOCTYPE x [<!ENTITY y "hidden">]><testsuite/>',
            '<testsuite tests="2"><testcase name="one"/></testsuite>',
            '<testsuite failures="1"><testcase name="one"/></testsuite>',
            '<testsuite skipped="1"><testcase name="one"/></testsuite>',
        ):
            report.write_text(text)
            with self.assertRaises(ValueError):
                source.read_junit(report, self.root)
        for child in ("failure", "error", "skipped", "flakyFailure"):
            report.write_text(
                f'<testsuite><testcase name="one" classname="file"><{child}/></testcase></testsuite>'
            )
            self.assertEqual(source.read_junit(report, self.root)[0]["verdict"], "FAIL")

    def test_per_test_retry_ignore_and_options_are_outside_the_fixed_host_plan(self):
        for path in source.GROUPS["deno-host"]:
            file = self.root / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text('Deno.test("one", () => {});')
        source.audit_deno_subset(self.root)
        file.write_text('Deno.test({name: "one", retry: 2, fn: () => {}});')
        with self.assertRaises(ValueError):
            source.audit_deno_subset(self.root)
        file.write_text('Deno.test("one", {ignore: true}, () => {});')
        with self.assertRaises(ValueError):
            source.audit_deno_subset(self.root)

    def test_all_publication_kinds_and_five_native_targets_remain_unverified(self):
        source.validate_coverage(self.coverage)
        for kind in source.ARTIFACT_TARGETS:
            changed = copy.deepcopy(self.coverage)
            changed["artifacts"] = [
                e for e in changed["artifacts"] if e["kind"] != kind
            ]
            with self.assertRaises(ValueError):
                source.validate_coverage(changed)
        for value in (True, 0, "false"):
            with self.assertRaises(ValueError):
                source.validate_coverage({
                    **self.coverage,
                    "publicationAuthorized": value,
                })
        changed = copy.deepcopy(self.coverage)
        changed["artifacts"][0]["verdict"] = "PASS"
        with self.assertRaises(ValueError):
            source.validate_coverage(changed)
        changed["artifacts"][0]["verdict"] = "UNVERIFIED"
        changed["artifacts"][0]["targets"].pop()
        with self.assertRaises(ValueError):
            source.validate_coverage(changed)

    def test_policy_json_rejects_duplicate_fields_and_boolean_schema(self):
        file = self.root / "plan.json"
        file.write_text('{"stage":"shadow","stage":"required"}')
        with self.assertRaises(ValueError):
            source.read_json(file)
        with self.assertRaises(ValueError):
            source.validate_plan({**self.plan, "schemaVersion": True})

    def test_children_do_not_inherit_credentials_or_runner_options(self):
        with patch.dict(
            os.environ,
            {"GH_TOKEN": "fixture-secret", "NODE_OPTIONS": "bad", "DENO_FLAGS": "bad"},
        ):
            env = source.child_environment()
            self.assertNotIn("GH_TOKEN", env)
            self.assertNotIn("NODE_OPTIONS", env)
            self.assertNotIn("DENO_FLAGS", env)
        code = source.execute(
            [sys.executable, "-c", "print('bounded source fixture')"],
            self.root / "child.log",
            self.root,
        )
        self.assertEqual(code, 0)

    def test_timeout_kills_the_owned_process_group(self):
        marker = self.root / "descendant-survived"
        child = f"import time,pathlib; time.sleep(.2); pathlib.Path({str(marker)!r}).write_text('bad')"
        parent = f"import subprocess,sys,time; subprocess.Popen([sys.executable,'-c',{child!r}]); time.sleep(10)"
        with self.assertRaisesRegex(ValueError, "timed out"):
            source.execute(
                [sys.executable, "-c", parent],
                self.root / "timeout.log",
                self.root,
                timeout=0.1,
            )
        time.sleep(0.25)
        self.assertFalse(marker.exists())

    def test_reusing_an_output_log_cannot_erase_a_first_failure(self):
        log = self.root / "first.log"
        log.write_text("first FAIL\n")
        with self.assertRaises(FileExistsError):
            source.execute([sys.executable, "-c", "print('PASS')"], log, self.root)
        self.assertEqual(log.read_text(), "first FAIL\n")

    def test_bad_source_or_host_still_preserves_a_fail_receipt(self):
        head = source.subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=source.ROOT, text=True
        ).strip()
        for name, expected in (("missing", ""), ("mismatch", "0" * 40)):
            output = self.root / name
            self.assertEqual(source.run_source(output, expected), 1)
            receipt = source.read_json(output / "source-receipt.json")
            self.assertEqual(receipt["sourceVerdict"], "FAIL")
            self.assertEqual(receipt["executedCases"], 0)
            self.assertIs(receipt["publicationAuthorized"], False)
        with patch.object(source.platform, "machine", return_value="aarch64"):
            output = self.root / "foreign-host"
            self.assertEqual(source.run_source(output, head), 1)
            self.assertIn(
                "Linux x86_64",
                source.read_json(output / "source-receipt.json")["error"],
            )


if __name__ == "__main__":
    unittest.main()
