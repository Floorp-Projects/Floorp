#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Metadata rejection tests using actual historical proof, never native qualification."""
import copy
from collections import Counter
import importlib.util
import json
import tempfile
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
DIRECTORY = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("qualification", DIRECTORY / "validate-qualification-evidence.py")
qualification = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qualification)


class QualificationEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.registry = json.loads((DIRECTORY / "qualification-evidence.json").read_text())

    def counts(self):
        self.registry["operationStatusCounts"] = {
            browser: dict(Counter(row[browser]["operationStatus"] for row in self.registry["extensions"]))
            for browser in ("chrome", "floorp")}

    def validate(self, **options):
        return qualification.validate(ROOT, self.registry, **options)

    def actual_report(self, path):
        absolute = ROOT / path
        if not absolute.is_file():
            self.skipTest("Local immutable browser proof not present: " + path)
        return json.loads(absolute.read_text())

    def test_current_complete_registry_never_qualifies_historical_proof(self):
        self.assertEqual(self.validate()["selected"], 50)
        self.assertEqual(self.validate()["nativeQualifiedCount"], 0)
        self.assertEqual(self.validate()["candidateGroups"], [])

    def test_historical157_promotion_rejected(self):
        row = next(row for row in self.registry["extensions"] if row["floorp"].get("reportPath"))
        row["nativeQualified"] = row["floorp"]["nativeQualified"] = True
        self.registry["nativeQualifiedCount"] = 1
        with self.assertRaisesRegex(ValueError, "Historical157"):
            self.validate()

    def test_relabelling_old_proof_as_candidate_requires_real_authentication(self):
        row = next(row for row in self.registry["extensions"] if row["floorp"].get("reportPath"))
        row["floorp"]["buildScope"] = qualification.NATIVE_SCOPE
        row["nativeQualified"] = row["floorp"]["nativeQualified"] = True
        with self.assertRaisesRegex(ValueError, "authenticated package"):
            self.validate()

    def test_unavailable_rows_cannot_shrink_denominator(self):
        self.registry["selection"]["selectedDenominator"] = 46
        with self.assertRaisesRegex(ValueError, "denominator"):
            self.validate()

    def test_unobserved_count_and_release_claim_rejected(self):
        for key, value, message in (("nativeQualifiedCount", 1, "count"),
                                    ("nativeQualified", True, "Release qualification"),
                                    ("primaryOperationPassRate", 90, "percentage")):
            with self.subTest(key=key):
                previous = self.registry[key]
                self.registry[key] = value
                with self.assertRaisesRegex(ValueError, message):
                    self.validate()
                self.registry[key] = previous

    def test_account_gate_cannot_become_primary_success(self):
        row = next(row for row in self.registry["extensions"] if row["name"] == "Grammarly")
        row["chrome"]["operationStatus"] = "primary-passed"
        self.counts()
        with self.assertRaisesRegex(ValueError, "prerequisite gate"):
            self.validate()

    def test_inventory_cannot_become_primary_success(self):
        row = next(row for row in self.registry["extensions"] if row["name"] == "Vue.js devtools")
        row["chrome"] = copy.deepcopy(row["history"][0]["observation"])
        row["chrome"]["operationStatus"] = "primary-passed"
        self.counts()
        with self.assertRaisesRegex(ValueError, "Inventory"):
            self.validate()

    def test_unsafe_current_and_historical_paths_rejected(self):
        for historical in (False, True):
            with self.subTest(historical=historical):
                original = copy.deepcopy(self.registry)
                row = next(row for row in self.registry["extensions"] if row["name"] == "Vue.js devtools")
                evidence = row["history"][0]["observation"] if historical else row["chrome"]
                evidence["reportPath"] = "../outside/report.json"
                with self.assertRaisesRegex(ValueError, "Unsafe evidence path"):
                    self.validate()
                self.registry = original

    def test_current_actual_artifact_hashes_and_changed_digest_rejected(self):
        for row in self.registry["extensions"]:
            for browser in ("chrome", "floorp"):
                path = row[browser].get("reportPath")
                if path:
                    self.actual_report(path)
        self.assertEqual(self.validate(check_artifacts=True)["nativeQualifiedCount"], 0)
        row = next(row for row in self.registry["extensions"] if row["name"] == "Vue.js devtools")
        row["chrome"]["reportSha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "digest mismatch"):
            self.validate(check_artifacts=True)

    def test_actual_report_duplicate_id_rejected(self):
        row = next(row for row in self.registry["extensions"] if row["name"] == "Vue.js devtools")
        report = self.actual_report(row["chrome"]["reportPath"])
        self.assertEqual(qualification.report_item(report, row["id"])["version"], row["version"])
        report["extensions"].append(copy.deepcopy(report["extensions"][0]))
        with self.assertRaisesRegex(ValueError, "exactly one"):
            qualification.report_item(report, row["id"])

    def test_actual_blocked_sider_reference_cannot_be_promoted_by_curated_status(self):
        row = next(row for row in self.registry["extensions"] if row["name"] == "Sider")
        self.actual_report(row["chrome"]["reportPath"])
        row["chrome"]["operationStatus"] = "primary-passed"
        self.counts()
        with self.assertRaisesRegex(ValueError, "Exact primary adapter result"):
            qualification.check_reference(ROOT, row["chrome"], row, check_artifacts=True)

    def test_actual_vue_success_cannot_substitute_for_react_reference(self):
        vue = next(row for row in self.registry["extensions"] if row["name"] == "Vue.js devtools")
        react = next(row for row in self.registry["extensions"] if row["name"] == "React Developer Tools")
        self.actual_report(vue["chrome"]["reportPath"])
        with self.assertRaisesRegex(ValueError, "exactly one"):
            qualification.check_reference(ROOT, vue["chrome"], react, check_artifacts=True)

    def test_reference_version_cannot_be_relabelled(self):
        self.registry["extensions"][0]["chrome"]["runtime"]["version"] = "999.0"
        with self.assertRaisesRegex(ValueError, "fixed official reference"):
            self.validate()

    def test_actual_crx_inventory_matches_recorded_signed_script_bytes(self):
        lock = json.loads((DIRECTORY / "corpus-lock.json").read_text())
        names = {"React Developer Tools", "Redux DevTools", "Dark Reader", "Stylus",
                 "Return YouTube Dislike", "Vue.js devtools", "LanguageTool", "Grammarly", "Ask Steve"}
        for selected in lock["extensions"]:
            if selected["name"] not in names:
                continue
            with self.subTest(extension=selected["name"]):
                package = ROOT / "_dist/chrome-extension-corpus/packages" / (selected["id"] + ".crx")
                if not package.is_file():
                    self.skipTest("Local immutable signed corpus packages not present")
                expected = qualification.original_inventory(ROOT, selected)
                row = next(row for row in self.registry["extensions"] if row["id"] == selected["id"])
                actual = row["chrome"].get("recordedScriptIntegrity") or row["floorp"]["recordedScriptIntegrity"]
                self.assertEqual(actual, {key: expected[key] for key in ("unchangedScriptCount", "unchangedScriptsSha256")})

    def test_all_actual_chrome_primary_ids_have_reviewed_native_adapters(self):
        completed = {row["id"] for row in self.registry["extensions"]
                     if row["chrome"]["operationStatus"] == "primary-passed"}
        supported = set().union(*(adapter["ids"] for adapter in qualification.SUITES.values()))
        self.assertEqual(len(completed), 26)
        self.assertLessEqual(completed, supported)
        for row in self.registry["extensions"]:
            if row["id"] not in completed:
                continue
            with self.subTest(extension=row["name"]):
                self.actual_report(row["chrome"]["reportPath"])
                qualification.check_reference(ROOT, row["chrome"], row, check_artifacts=True)

    def test_new_adapters_reject_missing_actual_primary_observations(self):
        cases = (("Awesome Screenshot", "media", "decoded authored-page PNG"),
                 ("GoFullPage", "steps", "Download control"),
                 ("LanguageTool", "afterNavigation", "both real documents"),
                 ("SponsorBlock", "afterRemoval", "document observations"),
                 ("Cookie Editor", "checks", "checks missing"),
                 ("OneTab", "snapshots", "both authored tabs"),
                 ("Session Buddy", "snapshots", "both authored tabs"),
                 ("Extensity", "checks", "checks missing"),
                 ("Momentum", "snapshots", "reloaded dashboard"))
        for name, missing, message in cases:
            with self.subTest(extension=name):
                row = next(row for row in self.registry["extensions"] if row["name"] == name)
                report = self.actual_report(row["chrome"]["reportPath"])
                item = qualification.report_item(report, row["id"])
                item.pop(missing, None)
                adapter = next(value for value in qualification.SUITES.values() if row["id"] in value["ids"])
                with self.assertRaisesRegex(ValueError, message):
                    qualification.check_reference_primary(ROOT, row, adapter, report, item)

    def test_actual_first_document_cannot_substitute_for_navigation(self):
        for name in ("LanguageTool", "SponsorBlock"):
            with self.subTest(extension=name):
                row = next(row for row in self.registry["extensions"] if row["name"] == name)
                report = self.actual_report(row["chrome"]["reportPath"])
                item = qualification.report_item(report, row["id"])
                item["afterNavigation"] = copy.deepcopy(item["transformed"])
                adapter = next(value for value in qualification.SUITES.values() if row["id"] in value["ids"])
                with self.assertRaisesRegex(ValueError, "both real documents|distinct real authored navigation"):
                    qualification.check_reference_primary(ROOT, row, adapter, report, item)

    def test_capture_reference_cannot_substitute_a_different_saved_file_digest(self):
        row = next(row for row in self.registry["extensions"] if row["name"] == "GoFullPage")
        report = self.actual_report(row["chrome"]["reportPath"])
        item = qualification.report_item(report, row["id"])
        item["media"]["sha256"] = "0" * 64
        adapter = next(value for value in qualification.SUITES.values() if row["id"] in value["ids"])
        with self.assertRaisesRegex(ValueError, "digest mismatch"):
            qualification.check_reference_primary(ROOT, row, adapter, report, item)

    def test_saved_capture_requires_real_decodable_png_and_authored_pixels(self):
        row = next(row for row in self.registry["extensions"] if row["name"] == "GoFullPage")
        report = self.actual_report(row["chrome"]["reportPath"])
        item = qualification.report_item(report, row["id"])
        data = Path(item["media"]["path"]).read_bytes()
        self.assertEqual(qualification.decode_capture_png(data), (item["media"]["width"], 2590, [41, 126, 216, 255]))
        with self.assertRaisesRegex(ValueError, "CRC|chunk|PNG"):
            qualification.decode_capture_png(data[:24] + bytes(len(data) - 24))
        with self.assertRaisesRegex(ValueError, "CRC|chunk|PNG"):
            qualification.decode_capture_png(data[:-12])
        report["output"] = str(ROOT / "_dist/chrome-runtime-tests/different-report")
        with self.assertRaises((ValueError, FileNotFoundError)):
            qualification.check_saved_capture(ROOT, report, item, native=False)

    def test_real_bundles_bind_all_imported_sources_and_copied_actors(self):
        spec = importlib.util.spec_from_file_location("browser_provenance", DIRECTORY / "run-browser-tests.py")
        producer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(producer)
        esbuild = next(iter(sorted((ROOT / "node_modules/.deno").glob("esbuild@*/node_modules/esbuild/bin/esbuild"))), None)
        if esbuild is None:
            self.skipTest("Actual project esbuild dependency not present")
        directory = ROOT / "_dist/chrome-runtime-tests"
        directory.mkdir(parents=True, exist_ok=True)
        for suite in ("tools/chrome-extension-runtime/test-corpus-languagetool-floorp.mjs",
                      "browser-features/modules/modules/chrome-web-store/test/polyfills/capture/CaptureCorpusNative.test.mts"):
            with self.subTest(suite=suite), tempfile.TemporaryDirectory(prefix="qualification-input-test-", dir=directory) as output:
                output = Path(output)
                modules = output / "modules"
                modules.mkdir()
                globals_path = modules / "browser-globals.mjs"
                globals_path.write_bytes(qualification.reviewed_browser_globals(ROOT))
                bundle = modules / "test-0.mjs"
                source = (ROOT / suite).resolve(strict=True)
                actual = producer.bundle_test(esbuild, source, globals_path, bundle, modules / "test-0.meta.json")
                # These are actual host bundles, without a browser/candidate or
                # any fabricated successful primary-operation fields.
                qualification.check_bundle_inputs(ROOT, output / "report.json", 0, actual, source, bundle)
                self.assertGreater(len(actual["bundleInputs"]), 1)
                changed = copy.deepcopy(actual)
                imported = next(item for item in changed["bundleInputs"] if item["path"] not in (str(source), str(globals_path)))
                imported["sha256"] = "0" * 64
                with self.assertRaisesRegex(ValueError, "imported bundle source"):
                    qualification.check_bundle_inputs(ROOT, output / "report.json", 0, changed, source, bundle)
                missing = copy.deepcopy(actual)
                missing["bundleInputs"] = missing["bundleInputs"][:-1]
                with self.assertRaisesRegex(ValueError, "every exact esbuild input"):
                    qualification.check_bundle_inputs(ROOT, output / "report.json", 0, missing, source, bundle)
                actors = [producer.copy_actor_module(DIRECTORY / name,
                          output / "profile/chrome/runtime-actors", modules / "actors")
                          for name in qualification.SUITE_ACTORS[qualification.SUITES[suite]["kind"]]]
                wrapper = {"modules": [{"path": str(source)}], "actorModules": actors}
                qualification.check_actor_sources(ROOT, output / "report.json", wrapper)
                if actors:
                    actor_copy = Path(actors[0]["copiedPath"])
                    actor_copy.write_bytes(actor_copy.read_bytes() + b"\n// changed retained actor\n")
                    with self.assertRaisesRegex(ValueError, "actor copy differs"):
                        qualification.check_actor_sources(ROOT, output / "report.json", wrapper)

    def test_old_wrapper_without_run_and_source_provenance_rejected(self):
        path = "_dist/chrome-runtime-tests/run-svi0xs13/report.json"
        wrapper = self.actual_report(path)
        # Actual historical wrapper has no per-browser UUID/source/bundle binding.
        with self.assertRaisesRegex(ValueError, "runId"):
            qualification.check_suite_provenance(ROOT, ROOT / path, wrapper,
                "tools/chrome-extension-runtime/test-corpus-devtools-floorp.mjs")


if __name__ == "__main__":
    unittest.main()
