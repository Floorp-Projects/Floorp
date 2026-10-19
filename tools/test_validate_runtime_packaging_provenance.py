# SPDX-License-Identifier: MPL-2.0

import copy
import unittest

from test_validate_runtime_source_provenance import PIN, RUNTIME_SHA
from validate_runtime_packaging_provenance import validate_packaging_provenance

BUILD_ID = "20261007125927"
INI = f"[App]\nVersion=157.0.1\nBuildID={BUILD_ID}\n".encode()
RECORD = {
    "version": "157.0.1", "build_id": BUILD_ID, "compiled_source_commit": RUNTIME_SHA,
    "original_resource_format": "flat", "resource_format": "omni",
    "resource_files_preserved": 8006, "native_binaries_unchanged": True,
    "native_binaries_sha256": {
        "Contents/MacOS/floorp": "a" * 64, "Contents/MacOS/XUL": "b" * 64,
    }, "gre_omnijar_bytes": 15600214,
}


class PackagingProvenanceTests(unittest.TestCase):
    def check_record(self, record, ini=INI):
        validate_packaging_provenance(record, PIN, RUNTIME_SHA, BUILD_ID, ini)

    def test_accepts_preserved_binaries_and_exact_authenticated_identity(self):
        self.check_record(RECORD)

    def test_rejects_missing_unknown_and_wrong_identity_fields(self):
        changes = {"version": "157.0", "build_id": "20261006125927",
                   "compiled_source_commit": "c" * 40, "resource_format": "flat",
                   "original_resource_format": "unknown", "native_binaries_unchanged": False}
        for key, value in changes.items():
            with self.subTest(field=key):
                record = copy.deepcopy(RECORD)
                record[key] = value
                with self.assertRaises(ValueError):
                    self.check_record(record)
        for key in RECORD:
            record = copy.deepcopy(RECORD)
            del record[key]
            with self.assertRaises(ValueError):
                self.check_record(record)
        record = {**RECORD, "ignored": True}
        with self.assertRaises(ValueError):
            self.check_record(record)

    def test_rejects_empty_boolean_and_noninteger_counts(self):
        for key in ("resource_files_preserved", "gre_omnijar_bytes"):
            for value in (0, -1, True, False, "8006", None):
                with self.subTest(field=key, value=value):
                    record = {**RECORD, key: value}
                    with self.assertRaises(ValueError):
                        self.check_record(record)

    def test_rejects_missing_native_inventory_and_invalid_hashes(self):
        for binaries in ({}, {"Contents/MacOS/floorp": "a" * 64},
                         {"Contents/MacOS/floorp": "x" * 64, "Contents/MacOS/XUL": "b" * 64},
                         {"Contents/MacOS/floorp": True, "Contents/MacOS/XUL": "b" * 64}):
            with self.assertRaises(ValueError):
                self.check_record({**RECORD, "native_binaries_sha256": binaries})

    def test_rejects_escaping_and_noncanonical_native_paths(self):
        for path in ("/Contents/bin", "Contents/../bin", "Contents//bin",
                     "Contents/./bin", "Contents\\bin", "Contents/bin\0"):
            record = copy.deepcopy(RECORD)
            record["native_binaries_sha256"][path] = "c" * 64
            with self.assertRaises(ValueError):
                self.check_record(record)

    def test_rejects_stale_duplicate_missing_and_oversized_ini(self):
        for ini in (INI.replace(b"157.0.1", b"157.0"), INI + b"BuildID=20261006125927\n",
                    b"[App]\nVersion=157.0.1\n", b" " * (32 * 1024 + 1), b"\xff"):
            with self.assertRaises(ValueError):
                self.check_record(RECORD, ini)

    def test_rejects_case_normalized_identity_keys(self):
        for ini in (INI.replace(b"Version", b"version"),
                    INI.replace(b"BuildID", b"buildid")):
            with self.subTest(ini=ini), self.assertRaises(ValueError):
                self.check_record(RECORD, ini)

    def test_rejects_identity_inherited_from_default_section(self):
        for ini in (INI.replace(b"[App]", b"[DEFAULT]") + b"[App]\n",
                    b"[DEFAULT]\nVersion=157.0.1\n[App]\nBuildID=" + BUILD_ID.encode() + b"\n",
                    b"[DEFAULT]\nBuildID=" + BUILD_ID.encode() + b"\n[App]\nVersion=157.0.1\n"):
            with self.subTest(ini=ini), self.assertRaises(ValueError):
                self.check_record(RECORD, ini)

    def test_app_local_identity_overrides_unrelated_default_section(self):
        self.check_record(RECORD, b"[DEFAULT]\nVersion=157.0\nBuildID=20261006125927\n" + INI)

    def test_rejects_untrusted_commit_and_invalid_expected_timestamp(self):
        for sha, build in (("branch", BUILD_ID), (RUNTIME_SHA, "20261307125927"),
                           (RUNTIME_SHA, "not-a-build-id")):
            with self.assertRaises(ValueError):
                validate_packaging_provenance(RECORD, PIN, sha, build, INI)


if __name__ == "__main__":
    unittest.main()
