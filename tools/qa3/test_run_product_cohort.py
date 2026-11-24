import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from unittest import TestCase

BRIDGE = Path(__file__).resolve().parents[1] / "qa" / "native-bridge"
sys.path.insert(0, str(BRIDGE))
SPEC = importlib.util.spec_from_file_location("run_product_cohort", BRIDGE / "run_product_cohort.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)

JOB = {"id": "tabs-001", "cases": ["a/browser_a.js", "a/browser_b.js"], "original_manifest": "a/browser.toml"}


def raw(rows):
    tmp = tempfile.NamedTemporaryFile("w", suffix=".jsonl", delete=False)
    tmp.write("\n".join(json.dumps(r) for r in rows) + "\n")
    tmp.close()
    return runner.read_raw(Path(tmp.name))


def ran(test, status="OK", **extra):
    return [{"action": "test_start", "test": test},
            {"action": "test_end", "test": test, "status": status, **extra}]


class TestRunner(TestCase):
    def batch(self, record, **overrides):
        result = {"exit": 0, "suite_end": True, "teardown_survivors": [], "leaks": [],
                  "unplanned_tests": [], "outcomes": runner.classify(JOB, record, 0)}
        result.update(overrides)
        return result

    def test_all_pass(self):
        record = raw([*ran(JOB["cases"][0]), *ran(JOB["cases"][1]), {"action": "suite_end"}])
        self.assertEqual(runner.verdict([self.batch(record)], 2)["verdict"], "PASS")

    def test_missing_and_failing_cases_fail_closed(self):
        cases = JOB["cases"]
        for rows in (
            ran(cases[0]),
            [*ran(cases[0]), *ran(cases[1], "FAIL", expected="OK")],
            [*ran(cases[0]), *ran(cases[1]),
             {"action": "test_status", "test": cases[1], "status": "FAIL", "expected": "PASS"}],
            [*ran(cases[0]), *ran(cases[1]), *ran(cases[1])],
            [*ran(cases[0]), *ran(cases[1]), {"action": "crash", "test": cases[1]}],
            [*ran(cases[0]), {"action": "test_start", "test": cases[1]}],
        ):
            with self.subTest(rows=rows):
                record = raw(rows)
                self.assertEqual(runner.verdict([self.batch(record)], 2)["verdict"], "FAIL")

    def test_batch_level_evidence_gaps_fail_closed(self):
        record = raw([*ran(JOB["cases"][0]), *ran(JOB["cases"][1])])
        for overrides in ({"exit": 1}, {"exit": "timeout"}, {"suite_end": False},
                          {"teardown_survivors": [42]}, {"leaks": [{"bytes": 8}]},
                          {"unplanned_tests": ["x.js"]}):
            with self.subTest(overrides=overrides):
                self.assertEqual(runner.verdict([self.batch(record, **overrides)], 2)["verdict"], "FAIL")
        self.assertEqual(runner.verdict([self.batch(record)], 3)["verdict"], "FAIL")
        self.assertEqual(runner.verdict([self.batch(None)], 2)["counts"], {"unexecuted": 2})
