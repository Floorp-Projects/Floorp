"""Host-only checks for the original Firefox product cohort planner."""

import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from unittest import TestCase

BRIDGE = Path(__file__).resolve().parents[1] / "qa" / "native-bridge"
SPEC = importlib.util.spec_from_file_location("product_cohort", BRIDGE / "product_cohort.py")
cohort = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cohort)


class TestProductCohort(TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.plan = json.loads(cohort.PLAN.read_text(encoding="utf-8"))
        self.fixture = copy.deepcopy(self.plan)
        for item in self.fixture["cohorts"]:
            manifest = self.root / item["manifest"]
            manifest.parent.mkdir(parents=True, exist_ok=True)
            for path in item["tests"]:
                (self.root / path).touch()
            raw = "".join(f'["{Path(path).name}"]\n' for path in item["tests"]).encode()
            manifest.write_bytes(raw)
            item["manifest_sha256"] = hashlib.sha256(raw).hexdigest()
        (self.root / "mach").touch()
        self.modules = self.root / "modules"
        self.modules.mkdir()
        self.product = self.root / "floorp"
        self.product.touch()

    def test_all_original_tests_once_in_serial_plan(self):
        cohorts = cohort.materialize(self.fixture, self.root)
        jobs = cohort.commands(
            cohorts, self.root, Path(sys.executable), self.product,
            self.modules, self.root / "evidence", 16,
        )
        cases = [case for job in jobs for case in job["cases"]]
        self.assertEqual([len(c["tests"]) for c in self.plan["cohorts"]], [127, 92, 70])
        self.assertEqual(cases, [p for c in self.plan["cohorts"] for p in c["tests"]])
        self.assertEqual(len(set(cases)), 289)
        self.assertEqual(len(jobs), 19)
        self.assertFalse((self.root / "evidence").exists())
        for job in jobs:
            self.assertIn(str(self.product), job["argv"])
            self.assertNotIn("--setpref", job["argv"])
            self.assertEqual(job["argv"][-len(job["cases"]):], job["cases"])

    def test_plan_is_pinned_to_locked_runtime(self):
        lock = json.loads((cohort.PLAN.parents[3] / "floorp-runtime.lock.json").read_text(encoding="utf-8"))
        self.assertEqual(self.plan["source_runtime"], lock["source"]["commit"])

    def test_changed_manifest_fails(self):
        (self.root / self.fixture["cohorts"][0]["manifest"]).write_text("")
        with self.assertRaisesRegex(ValueError, "manifest changed"):
            cohort.materialize(self.fixture, self.root)

    def test_unregistered_and_duplicate_entries_fail(self):
        cases = self.fixture["cohorts"][0]["tests"]
        cases.append(cases[-1])
        with self.assertRaisesRegex(ValueError, "duplicated"):
            cohort.materialize(self.fixture, self.root)
        cases.pop()
        cases.insert(0, "browser/components/tabbrowser/test/browser/tabs/browser_0000_not_registered.js")
        with self.assertRaisesRegex(ValueError, "not uniquely registered"):
            cohort.materialize(self.fixture, self.root)

    def test_missing_case_fails(self):
        (self.root / self.fixture["cohorts"][0]["tests"][0]).unlink()
        with self.assertRaises(FileNotFoundError):
            cohort.materialize(self.fixture, self.root)

    def test_runtime_binary_cannot_be_used_as_default_product(self):
        firefox = self.root / "firefox"
        firefox.touch()
        with self.assertRaisesRegex(ValueError, "explicit Floorp"):
            cohort.commands(
                self.fixture["cohorts"], self.root, Path(sys.executable),
                firefox, self.modules, self.root / "evidence", 16,
            )

    def test_no_singleton_batch(self):
        cohorts = cohort.materialize(self.fixture, self.root)
        jobs = cohort.commands(
            cohorts, self.root, Path(sys.executable), self.product,
            self.modules, self.root / "evidence", 14,
        )
        self.assertTrue(all(2 <= len(j["cases"]) <= 14 for j in jobs))
        self.assertEqual(
            [path for j in jobs for path in j["cases"]],
            [path for c in cohorts for path in c["tests"]],
        )
        self.assertTrue(all("--dump-tests" in j["discovery_argv"] for j in jobs))

    def test_stock_discovery_requires_exact_active_cases(self):
        item = self.fixture["cohorts"][0]
        job = {"cases": item["tests"][:2], "original_manifest": item["manifest"]}
        active = [{"path": path, "manifest": item["manifest"], "expected": "pass"}
                  for path in job["cases"]]
        cohort.validate_discovery(job, {"active_tests": active[::-1]})
        cohort.validate_discovery(job, {"active_tests": [
            {**row, "manifest": "browser.toml"} for row in active
        ]})
        for records in (
            active[:1], [active[0], active[0]],
            [active[0], {**active[1], "disabled": "debug"}],
            [active[0], {**active[1], "manifest": "browser_oldformat.toml"}],
            [active[0], {**active[1], "expected": "fail"}],
            [*active, {**active[0], "path": "unexpected.js"}],
        ):
            with self.subTest(records=records), self.assertRaises(ValueError):
                cohort.validate_discovery(job, {"active_tests": records})
