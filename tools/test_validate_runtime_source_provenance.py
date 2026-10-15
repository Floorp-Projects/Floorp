# SPDX-License-Identifier: MPL-2.0

import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from validate_runtime_source_provenance import read_json, validate_source_provenance

RUNTIME_SHA = "b" * 40
PATCH = b"reviewed native patch\n"
PIN = {
    "schema_version": 1,
    "upstream": {
        "repository": "mozilla-firefox/firefox",
        "version": "157.0.1",
        "tag": "FIREFOX_157_0_1_RELEASE",
        "commit": "0c469c2352451630bc69fc328c9f0c589c6c534d",
        "previous_tag": "FIREFOX_157_0_RELEASE",
        "previous_commit": "fdd757a2e09c9471cddf383e64e631e4ce178499",
    },
    "floorp_patch": {
        "repository": "Floorp-Projects/Floorp",
        "commit": "a" * 40,
        "path": ".github/patches/floorp-runtime/common/reviewed.patch",
        "runtime_path": ".github/patches/upstream/reviewed.patch",
        "sha256": hashlib.sha256(PATCH).hexdigest(),
        "patched_files": {"widget/reviewed.cpp": "c" * 64},
    },
}


def record():
    return {**copy.deepcopy(PIN), "runtime_commit": RUNTIME_SHA}


def snapshot_pin():
    pin = copy.deepcopy(PIN)
    pin["upstream"].update({
        "version": "158.0.1",
        "tag": None,
        "tracking_ref": "refs/heads/release",
        "commit": "1a0cfe22a2d8ba77e77cc8f43eaa480e34eca181",
        "tree": "04e6d359951669601fc15746904f31bd906f87f4",
        "previous_tag": "FIREFOX_157_0_1_RELEASE",
        "previous_commit": PIN["upstream"]["commit"],
    })
    return pin


class SourceProvenanceTests(unittest.TestCase):
    def test_authenticates_exact_source_and_current_native_patch(self):
        validate_source_provenance(record(), PIN, RUNTIME_SHA, PATCH)

    def test_authenticates_exact_immutable_release_snapshot(self):
        pin = snapshot_pin()
        value = {**copy.deepcopy(pin), "runtime_commit": RUNTIME_SHA}
        validate_source_provenance(value, pin, RUNTIME_SHA, PATCH)
        # A later release tag may label the same immutable snapshot.
        pin["upstream"]["tag"] = "FIREFOX_158_0_1_RELEASE"
        value["upstream"]["tag"] = pin["upstream"]["tag"]
        validate_source_provenance(value, pin, RUNTIME_SHA, PATCH)

    def test_rejects_partial_unknown_and_invalid_snapshot_pins(self):
        for missing in ("tracking_ref", "tree"):
            pin = snapshot_pin()
            del pin["upstream"][missing]
            with self.subTest(missing=missing), self.assertRaises(ValueError):
                validate_source_provenance(
                    {**copy.deepcopy(pin), "runtime_commit": RUNTIME_SHA}, pin, RUNTIME_SHA, PATCH)
        changes = {
            "tracking_ref": ("release", "refs/heads/beta", "refs/heads/release\n", None),
            "tree": ("main", "d" * 39, "D" * 40, None, True),
            "commit": ("release", "a" * 39, None),
            "previous_commit": ("old-release", None),
            "version": ("158", "158.0.1\n", None, True),
            "tag": ("FIREFOX_157_0_1_RELEASE", "release", False),
            "ignored": ("extra",),
        }
        for field, values in changes.items():
            for change in values:
                pin = snapshot_pin()
                pin["upstream"][field] = change
                with self.subTest(field=field, value=change), self.assertRaises(ValueError):
                    validate_source_provenance(
                        {**copy.deepcopy(pin), "runtime_commit": RUNTIME_SHA}, pin, RUNTIME_SHA, PATCH)

    def test_rejects_every_snapshot_identity_mismatch_and_extra_record_field(self):
        pin = snapshot_pin()
        for field in (*pin["upstream"], "ignored"):
            value = {**copy.deepcopy(pin), "runtime_commit": RUNTIME_SHA}
            value["upstream"][field] = "different-source"
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "upstream source differs"):
                validate_source_provenance(value, pin, RUNTIME_SHA, PATCH)

    def test_rejects_another_run_commit(self):
        with self.assertRaisesRegex(ValueError, "commit differs"):
            validate_source_provenance(record(), PIN, "d" * 40, PATCH)

    def test_rejects_a_malformed_expected_commit(self):
        with self.assertRaisesRegex(ValueError, "canonical SHA"):
            validate_source_provenance(record(), PIN, "branch-name", PATCH)

    def test_rejects_incomplete_extra_and_boolean_schemas(self):
        for change in ("missing", "extra", "boolean", "new-version"):
            with self.subTest(change=change):
                value = record()
                if change == "missing":
                    del value["upstream"]
                elif change == "extra":
                    value["ignored"] = True
                else:
                    value["schema_version"] = True if change == "boolean" else 2
                with self.assertRaises(ValueError):
                    validate_source_provenance(value, PIN, RUNTIME_SHA, PATCH)

    def test_rejects_every_upstream_identity_mismatch(self):
        for field in PIN["upstream"]:
            with self.subTest(field=field):
                value = record()
                value["upstream"][field] = "another-source"
                with self.assertRaisesRegex(ValueError, "upstream source differs"):
                    validate_source_provenance(value, PIN, RUNTIME_SHA, PATCH)

    def test_rejects_patch_digest_material_and_unknown_fields(self):
        for field in PIN["floorp_patch"]:
            with self.subTest(field=field):
                value = record()
                value["floorp_patch"][field] = {} if field == "patched_files" else "changed"
                with self.assertRaisesRegex(ValueError, "patch differs"):
                    validate_source_provenance(value, PIN, RUNTIME_SHA, PATCH)
        value = record()
        value["floorp_patch"]["ignored"] = "extra"
        with self.assertRaisesRegex(ValueError, "patch differs"):
            validate_source_provenance(value, PIN, RUNTIME_SHA, PATCH)

    def test_rejects_changed_current_floorp_native_patch(self):
        with self.assertRaisesRegex(ValueError, "current Floorp patch"):
            validate_source_provenance(record(), PIN, RUNTIME_SHA, PATCH + b"changed")

    def test_rejects_duplicate_fields_links_and_oversized_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "record.json"
            path.write_text('{"upstream":{"version":"old","version":"new"}}')
            with self.assertRaisesRegex(ValueError, "duplicate JSON field"):
                read_json(path)
            path.write_text(json.dumps(record()))
            linked = Path(directory) / "linked.json"
            linked.symlink_to(path)
            with self.assertRaisesRegex(ValueError, "regular, unlinked"):
                read_json(linked)
            path.write_text(" " * (128 * 1024 + 1))
            with self.assertRaisesRegex(ValueError, "size limit"):
                read_json(path)


if __name__ == "__main__":
    unittest.main()
