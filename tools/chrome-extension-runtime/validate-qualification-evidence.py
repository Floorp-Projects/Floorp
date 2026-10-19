#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Validate the complete frozen selection and honest evidence/build boundaries."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DIRECTORY = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-artifacts", action="store_true",
                        help="Also verify the local report files and their recorded SHA256")
    args = parser.parse_args()
    lock_bytes = (DIRECTORY / "corpus-lock.json").read_bytes()
    lock = json.loads(lock_bytes)
    registry = json.loads((DIRECTORY / "qualification-evidence.json").read_text())
    assert registry["selection"]["lockSha256"] == hashlib.sha256(lock_bytes).hexdigest()
    assert registry["selection"]["selectionSha256"] == lock["selectionSha256"]
    assert len(registry["extensions"]) == len(lock["extensions"]) == 50
    assert registry["selection"]["selectedDenominator"] == 50
    assert registry["selection"]["unavailableIncludedInDenominator"] is True
    assert registry["selection"]["unavailablePackages"] == 4
    assert registry["nativeQualified"] is False
    assert registry["nativeQualifiedCount"] == 0
    assert registry["primaryOperationPassRate"] is None
    assert sum(row["packageStatus"] == "unavailable" for row in registry["extensions"]) == 4
    for browser in ("chrome", "floorp"):
        assert registry["operationStatusCounts"][browser] == dict(Counter(
            row[browser]["operationStatus"] for row in registry["extensions"]))
    for selected, row in zip(lock["extensions"], registry["extensions"]):
        for key in ("id", "name", "primaryOperation"):
            assert row[key] == selected[key], (row["id"], key)
        assert row["packageSha256"] == selected.get("packageSha256")
        assert row["nativeQualified"] is False
        assert row["owner"] and row["nextStep"] and row["blockerCategory"]
        for browser in ("chrome", "floorp"):
            evidence = row[browser]
            assert evidence["nativeQualified"] is False
            assert evidence["operationStatus"] and evidence["buildScope"]
            if evidence.get("evidenceKind") == "ui-inventory":
                assert evidence["operationStatus"] == "not-run"
            if evidence.get("evidenceKind") == "actual-package-prerequisite-only":
                assert evidence["primaryOperationAttempted"] is False
                assert evidence["operationStatus"] != "primary-passed"
                assert evidence["observedPrerequisiteStatus"]
            path = evidence.get("reportPath")
            if path:
                assert not Path(path).is_absolute() and ".." not in Path(path).parts
                assert evidence["runtime"]
                if browser == "floorp":
                    assert evidence["buildScope"] == "historical-prebuilt157-not-native158"
                    assert evidence["runtime"]["version"] == "157.0"
                if args.check_artifacts:
                    assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == evidence["reportSha256"], path
    print("PASS: all50 selected rows,4 included unavailable, exact frozen primaries; native158 qualified0")


if __name__ == "__main__":
    main()
