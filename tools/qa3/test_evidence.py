# SPDX-License-Identifier: MPL-2.0
"""Contract tests for EvidenceManifest v0 (KAN-106 / frozen ADR 8061068 1.1-1.5).

Covered here:

* schema + TypeScript-side fixture contract on the Python side,
* conversion of one QA3 receipt, three work records and the final-package QA /
  provenance records, all of which must pass the schema after conversion,
* rejection of unknown schemaVersion, invalid transitions, missing required
  evidence, cancel_requested mismatches, unknown entities, bad ID namespaces and
  old-epoch reports.
"""

import copy
import hashlib
import json
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from evidence import convert, validate  # noqa: E402  (path setup above)

REPO = HERE.parents[1]
FIXTURES = HERE / "evidence" / "fixtures"
WORK_RECORDS = REPO / "docs" / "qa" / "kit-v0.3.0-review-fixes" / "followup-evidence"

REQUIRED_GROUPS = [
    "schemaVersion",
    "envelope",
    "runtime",
    "attempts",
    "operations",
    "assertions",
    "environmentFidelity",
    "inputFidelity",
    "visualJudge",
    "cleanup",
    "limitations",
    "reproduction",
]

WORK_RECORD_FILES = [
    "docs-public-independent-close-02.json",
    "floorp-independent-negative-01.json",
    "floorp-source106-clean-commit-01.json",
]

EXTERNAL_ORIGINALS = {
    FIXTURES / "inputs" / "qa3-source-receipt.json": Path(
        "/tmp/fdh-gate1/qa3-shadow-release/source/source-receipt.json"
    ),
    FIXTURES / "inputs" / "final-package-native-verification.json": Path(
        "/tmp/fdh-gate1/qa-windows-42371731162/native-verification.json"
    ),
    FIXTURES / "inputs" / "final-package-web-panel-ua-report.json": Path(
        "/tmp/fdh-gate1/qa-windows-42371731162/web-panel-ua/run-lysuftz2/report.json"
    ),
}


def codes(errors):
    return [error["code"] for error in errors]


def normalized_bytes(path):
    """Repository `* text=auto` stores JSON as LF while two gate1 originals are
    CRLF, so provenance is compared after end-of-line normalisation."""
    return path.read_bytes().replace(b"\r\n", b"\n")


def load(relative):
    return validate.read_json(FIXTURES / relative)


class EvidenceSchemaContractTests(unittest.TestCase):
    def setUp(self):
        self.schema = validate.load_schema()
        self.transitions = validate.load_transitions()
        self.registry = load("registry.json")

    def test_schema_requires_every_adr_13_evidence_element(self):
        self.assertEqual(self.schema["$schema"], "http://json-schema.org/draft-07/schema#")
        self.assertEqual(self.schema["required"], REQUIRED_GROUPS)
        self.assertEqual(self.schema["properties"]["schemaVersion"]["const"], 1)
        self.assertEqual(
            self.schema["properties"]["reproduction"]["$ref"],
            "#/definitions/reproduction",
        )
        reproduction = self.schema["definitions"]["reproduction"]["properties"]
        self.assertEqual(
            reproduction["result"]["enum"],
            ["reproduced", "not-reproduced", "inconclusive"],
        )
        self.assertEqual(
            self.schema["definitions"]["visualJudge"]["properties"]["source"]["enum"],
            ["human", "agent"],
        )
        assertion = self.schema["definitions"]["assertion"]
        self.assertEqual(assertion["required"], ["name", "expected", "actual", "passed"])
        operation = self.schema["definitions"]["operation"]
        self.assertEqual(operation["required"], ["input", "target", "order", "observation"])
        attempt = self.schema["definitions"]["attempt"]
        self.assertEqual(
            attempt["required"], ["run", "attempt", "startedAt", "endedAt", "result"]
        )

    def test_state_transition_table_is_closed_and_names_cancel_requested(self):
        states = set(self.transitions["states"])
        self.assertEqual(set(self.transitions["transitions"]), states)
        for state, targets in self.transitions["transitions"].items():
            self.assertTrue(
                set(targets) <= states, f"{state} points outside the table"
            )
            self.assertNotIn(state, targets, "self transitions are not allowed")
        for terminal in self.transitions["terminalStates"]:
            self.assertEqual(self.transitions["transitions"][terminal], [])
        self.assertIn(self.transitions["cancelRequestedState"], states)
        self.assertEqual(self.transitions["initialStates"], ["pending"])

    def test_registry_entries_exist_and_are_unique(self):
        seen = set()
        for relative in self.registry["valid"]:
            self.assertNotIn(relative, seen, f"duplicate fixture {relative}")
            seen.add(relative)
            self.assertTrue((FIXTURES / relative).is_file(), relative)
        for entry in self.registry["invalid"]:
            relative = entry["path"]
            self.assertNotIn(relative, seen, f"duplicate fixture {relative}")
            seen.add(relative)
            self.assertTrue((FIXTURES / relative).is_file(), relative)
            self.assertIn("code", entry)
        # Every generated/invalid fixture on disk is registered.
        on_disk = {
            path.relative_to(FIXTURES).as_posix()
            for path in FIXTURES.rglob("*.json")
            if path.parent.name in {"valid", "invalid", "converted"}
        }
        self.assertEqual(on_disk, seen)

    def test_valid_fixtures_pass(self):
        for relative in self.registry["valid"]:
            with self.subTest(fixture=relative):
                self.assertEqual(validate.validate_manifest(load(relative)), [])

    def test_invalid_fixtures_are_rejected_with_the_registered_code(self):
        for entry in self.registry["invalid"]:
            with self.subTest(fixture=entry["path"]):
                manifest = load(entry["path"])
                errors = validate.validate_manifest(
                    manifest, current_epoch=entry.get("currentEpoch")
                )
                self.assertIn(entry["code"], codes(errors), errors)

    def test_unknown_schema_version_is_rejected(self):
        manifest = load("invalid/unknown-schema-version.json")
        errors = validate.validate_manifest(manifest)
        self.assertEqual(codes(errors), ["unsupported-schema-version"])
        self.assertEqual(errors[0]["path"], "/schemaVersion")
        # A missing version is missing required evidence, not a silent pass.
        missing = copy.deepcopy(load("valid/manifest-visual-judge-human.json"))
        missing.pop("schemaVersion")
        self.assertEqual(codes(validate.validate_manifest(missing)), [
            "missing-required-evidence"
        ])

    def test_invalid_transition_is_rejected(self):
        errors = validate.validate_manifest(load("invalid/invalid-transition.json"))
        self.assertIn("invalid-transition", codes(errors))
        # Terminal states have no outgoing transitions at all.
        terminal = load("valid/manifest-visual-judge-human.json")
        terminal["envelope"]["taskStatus"]["state"] = "succeeded"
        terminal["envelope"]["stateTransition"] = {
            "from": "succeeded", "to": "succeeded"
        }
        self.assertIn("invalid-transition", codes(validate.validate_manifest(terminal)))
        # A legal transition still validates.
        legal = load("valid/manifest-visual-judge-human.json")
        legal["envelope"]["stateTransition"] = {
            "from": "awaiting-approval",
            "to": "running",
            "at": "2026-11-07T21:10:00+09:00",
        }
        legal["envelope"]["taskStatus"]["state"] = "running"
        self.assertEqual(validate.validate_manifest(legal), [])

    def test_missing_required_evidence_is_rejected(self):
        base = load("valid/manifest-visual-judge-human.json")
        for field in ("runtime", "attempts", "operations", "assertions",
                      "environmentFidelity", "inputFidelity", "visualJudge",
                      "cleanup", "limitations", "reproduction", "envelope"):
            with self.subTest(field=field):
                broken = copy.deepcopy(base)
                broken.pop(field)
                errors = validate.validate_manifest(broken)
                self.assertEqual(codes(errors), ["missing-required-evidence"])
                self.assertEqual(errors[0]["path"], f"/{field}")

    def test_cancel_requested_must_agree_everywhere(self):
        errors = validate.validate_manifest(load("invalid/cancel-mismatch.json"))
        self.assertIn("cancel-state-mismatch", codes(errors))
        # state and flag must also agree with each other.
        flipped = load("valid/manifest-visual-judge-human.json")
        flipped["envelope"]["taskStatus"]["cancelRequested"] = True
        self.assertIn(
            "cancel-state-mismatch", codes(validate.validate_manifest(flipped))
        )
        agreed = load("valid/manifest-cancel-requested.json")
        self.assertEqual(validate.validate_manifest(agreed), [])

    def test_old_epoch_reports_are_rejected_by_central_intake_only(self):
        stale = load("invalid/stale-epoch.json")
        # Runner side: no central epoch, the record itself is well formed.
        self.assertEqual(validate.validate_manifest(stale), [])
        # Scheduler side: the report is older than the current epoch.
        self.assertEqual(
            codes(validate.validate_report_as_scheduler(stale, current_epoch=1)),
            ["stale-epoch"],
        )
        # Central intake without a known epoch refuses the report outright.
        self.assertEqual(
            codes(validate.validate_report_as_scheduler(stale, current_epoch=None)),
            ["missing-current-epoch"],
        )
        # A fresh epoch passes.
        fresh = load("valid/manifest-cancel-requested.json")
        self.assertEqual(
            validate.validate_report_as_scheduler(
                fresh, current_epoch=fresh["envelope"]["epoch"]
            ),
            [],
        )

    def test_id_issuer_namespaces_and_entity_kinds_are_enforced(self):
        manifest = load("invalid/wrong-id-namespace.json")
        self.assertIn("invalid-id-namespace", codes(validate.validate_manifest(manifest)))
        manifest = load("invalid/missing-session-entity.json")
        self.assertIn("missing-entity", codes(validate.validate_manifest(manifest)))
        manifest = load("invalid/unknown-entity-reference.json")
        self.assertIn(
            "unknown-entity-reference", codes(validate.validate_manifest(manifest))
        )
        for relative in self.registry["valid"]:
            with self.subTest(fixture=relative):
                envelope = load(relative)["envelope"]
                kinds = [entity["kind"] for entity in envelope["entities"]]
                self.assertEqual(
                    sorted(kinds),
                    sorted(validate.ENTITY_KINDS),
                    "every Envelope carries all seven主体 kinds",
                )
                for entity in envelope["entities"]:
                    namespace = validate.ISSUER_NAMESPACE[entity["issuer"]]
                    self.assertTrue(entity["id"].startswith(namespace))

    def test_visual_judge_source_is_human_or_agent(self):
        errors = validate.validate_manifest(load("invalid/visual-judge-source-robot.json"))
        self.assertIn("schema-violation", codes(errors))
        for relative in self.registry["valid"]:
            judge = load(relative)["visualJudge"]
            self.assertTrue(judge is None or judge["source"] in {"human", "agent"})


class ConverterTests(unittest.TestCase):
    def setUp(self):
        self.registry = validate.read_json(FIXTURES / "registry.json")

    def test_one_qa3_receipt_converts_and_passes_the_schema(self):
        source = FIXTURES / "inputs" / "qa3-source-receipt.json"
        manifest = convert.convert_qa3_receipt(validate.read_json(source))
        self.assertEqual(validate.validate_manifest(manifest), [])
        self.assertEqual(manifest["sourceFormat"], "qa3-receipt")
        receipt = validate.read_json(source)
        self.assertEqual(receipt["sourceVerdict"], "PASS")
        self.assertEqual(receipt["productVerdict"], "UNVERIFIED")
        self.assertEqual(receipt["plannedCases"], 109)
        self.assertEqual(len(manifest["attempts"]), 1)
        self.assertEqual(manifest["attempts"][0]["result"], "passed")
        self.assertEqual(manifest["envelope"]["taskStatus"]["state"], "succeeded")
        self.assertEqual(len([a for a in manifest["assertions"] if a["passed"]]),
                         len(manifest["assertions"]))
        self.assertTrue(
            any("DSH" in item for item in manifest["limitations"]),
            "UNVERIFIED DSH pin must be mirrored in limitations",
        )

    def test_three_work_records_convert_and_pass_the_schema(self):
        self.assertEqual(len(WORK_RECORD_FILES), 3)
        conversions = []
        for name in WORK_RECORD_FILES:
            with self.subTest(record=name):
                path = WORK_RECORDS / name
                self.assertTrue(path.is_file(), path)
                record = validate.read_json(path)
                manifest = convert.convert_work_record(record, name=path.stem)
                errors = validate.validate_manifest(manifest)
                self.assertEqual(errors, [])
                conversions.append((path.stem, manifest))
        results = {manifest["reproduction"]["result"] for _, manifest in conversions}
        self.assertEqual(
            results,
            {"reproduced", "not-reproduced", "inconclusive"},
            "the three work records must exercise the whole reproduction axis",
        )
        # One record carries real wall-clock attempt times.
        timed = [
            manifest
            for _, manifest in conversions
            if manifest["attempts"][0]["startedAt"] is not None
        ]
        self.assertEqual(len(timed), 1)

    def test_final_package_qa_and_provenance_records_convert(self):
        for name, expect_format in (
            ("final-package-native-verification.json", "final-package-qa"),
            ("final-package-web-panel-ua-report.json", "final-package-qa"),
        ):
            with self.subTest(record=name):
                manifest = convert.convert_final_package_record(
                    validate.read_json(FIXTURES / "inputs" / name)
                )
                self.assertEqual(validate.validate_manifest(manifest), [])
                self.assertEqual(manifest["sourceFormat"], expect_format)
                self.assertIsNotNone(manifest["runtime"]["packageDigest"])

    def test_all_three_formats_are_represented_in_the_registry(self):
        formats = set()
        for relative in self.registry["valid"]:
            manifest = load(relative)
            if "sourceFormat" in manifest:
                formats.add(manifest["sourceFormat"])
        self.assertEqual(
            formats, {"qa3-receipt", "final-package-qa", "work-record"}
        )

    def test_committed_converted_fixtures_match_a_fresh_conversion(self):
        for kind, source, output_name in convert.CONVERTED_FIXTURES:
            with self.subTest(fixture=output_name):
                fresh = convert.convert_path(kind, source)
                committed = validate.read_json(convert.CONVERTED / output_name)
                self.assertEqual(committed, fresh)
                self.assertEqual(validate.validate_manifest(committed), [])

    def test_conversion_does_not_mutate_the_source_record(self):
        for kind, source, _ in convert.CONVERTED_FIXTURES:
            with self.subTest(source=source.name):
                before = source.read_bytes()
                convert.convert_path(kind, source)
                self.assertEqual(source.read_bytes(), before)

    @unittest.skipUnless(
        any(path.exists() for path in EXTERNAL_ORIGINALS.values()),
        "gate1 source records are not mounted in this environment",
    )
    def test_vendored_inputs_match_the_gate1_originals(self):
        for vendored, original in EXTERNAL_ORIGINALS.items():
            if not original.exists():
                continue
            with self.subTest(input=vendored.name):
                self.assertEqual(
                    hashlib.sha256(normalized_bytes(vendored)).hexdigest(),
                    hashlib.sha256(normalized_bytes(original)).hexdigest(),
                )
                self.assertEqual(
                    json.loads(vendored.read_bytes()), json.loads(original.read_bytes())
                )

    def test_unknown_record_shapes_are_refused(self):
        with self.assertRaises(ValueError):
            convert.convert_work_record({"unexpected": True})
        with self.assertRaises(ValueError):
            convert.convert_final_package_record({"unexpected": True})


class SharedFixtureTests(unittest.TestCase):
    """Scheduler and Runner consume the same registry on the Python side."""

    def setUp(self):
        self.registry = validate.read_json(FIXTURES / "registry.json")

    def test_scheduler_and_runner_see_the_same_fixture_files(self):
        runner_files = [entry for entry in self.registry["valid"]] + [
            entry["path"] for entry in self.registry["invalid"]
        ]
        scheduler_files = list(runner_files)
        self.assertEqual(runner_files, scheduler_files)
        self.assertEqual(len(runner_files), len(set(runner_files)))

    def test_runner_and_scheduler_agree_except_for_the_epoch_rule(self):
        for entry in self.registry["invalid"]:
            with self.subTest(fixture=entry["path"]):
                manifest = validate.read_json(FIXTURES / entry["path"])
                current_epoch = entry.get("currentEpoch")
                if current_epoch is None:
                    # The centre always knows its epoch; for fixtures that are
                    # not epoch-related it is the manifest's own epoch.
                    current_epoch = manifest["envelope"]["epoch"]
                runner_codes = codes(validate.validate_manifest(manifest))
                scheduler_codes = codes(
                    validate.validate_report_as_scheduler(manifest, current_epoch)
                )
                if entry.get("runnerAccepts"):
                    # ADR 1.2: the old Runner keeps writing; only the centre
                    # rejects the stale epoch.
                    self.assertEqual(runner_codes, [])
                    self.assertEqual(scheduler_codes, ["stale-epoch"])
                else:
                    self.assertEqual(runner_codes, scheduler_codes)
                    self.assertIn(entry["code"], scheduler_codes)

    def test_valid_fixtures_pass_both_roles(self):
        for relative in self.registry["valid"]:
            manifest = validate.read_json(FIXTURES / relative)
            epoch = manifest["envelope"]["epoch"]
            with self.subTest(fixture=relative):
                self.assertEqual(validate.validate_manifest(manifest), [])
                self.assertEqual(
                    validate.validate_report_as_scheduler(manifest, epoch), []
                )


if __name__ == "__main__":
    unittest.main()
