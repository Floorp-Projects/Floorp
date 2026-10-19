#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0

import argparse
import configparser
import re
from datetime import datetime
from pathlib import Path, PurePosixPath

from validate_runtime_source_provenance import closed_object, read_json, read_regular


def validate_packaging_provenance(record, pin, runtime_head_sha, expected_build_id,
                                 application_ini):
    if not re.fullmatch(r"[0-9a-f]{40}", runtime_head_sha):
        raise ValueError("Expected Runtime commit is not a canonical SHA")
    if not re.fullmatch(r"[0-9]{14}", expected_build_id):
        raise ValueError("Expected BuildID is not a canonical UTC timestamp")
    datetime.strptime(expected_build_id, "%Y%m%d%H%M%S")
    closed_object(pin, ("schema_version", "upstream", "floorp_patch"), "Runtime pin")
    if type(pin["schema_version"]) is not int or pin["schema_version"] != 1:
        raise ValueError("Unsupported Runtime source pin version")
    if not isinstance(pin["upstream"], dict):
        raise ValueError("Invalid upstream pin")
    version = pin["upstream"].get("version")
    if not isinstance(version, str) or not re.fullmatch(r"[0-9]+\.[0-9]+(?:\.[0-9]+)?", version):
        raise ValueError("Invalid pinned upstream version")
    closed_object(record, (
        "version", "build_id", "compiled_source_commit", "original_resource_format",
        "resource_format", "resource_files_preserved", "native_binaries_unchanged",
        "native_binaries_sha256", "gre_omnijar_bytes",
    ), "Runtime packaging provenance")
    if record["compiled_source_commit"] != runtime_head_sha:
        raise ValueError("Packaged Runtime source commit differs from the authenticated run")
    if record["build_id"] != expected_build_id or record["version"] != version:
        raise ValueError("Packaged Runtime identity differs from the authenticated pin")
    if record["original_resource_format"] not in ("flat", "jar", "omni"):
        raise ValueError("Unsupported original Runtime resource format")
    if record["resource_format"] != "omni":
        raise ValueError("Packaged Runtime does not contain the required omnijar layout")
    if record["native_binaries_unchanged"] is not True:
        raise ValueError("Runtime repacking did not preserve compiled native binaries")
    for field in ("resource_files_preserved", "gre_omnijar_bytes"):
        if type(record[field]) is not int or record[field] <= 0:
            raise ValueError(f"Invalid Runtime packing count: {field}")
    binaries = record["native_binaries_sha256"]
    if not isinstance(binaries, dict) or not {
        "Contents/MacOS/floorp", "Contents/MacOS/XUL",
    }.issubset(binaries):
        raise ValueError("Runtime packaging inventory omits required native binaries")
    for name, digest in binaries.items():
        path = PurePosixPath(name)
        if (not name.startswith("Contents/") or path.is_absolute() or ".." in path.parts
                or str(path) != name or "\\" in name or "\0" in name):
            raise ValueError("Invalid relative native binary inventory path")
        if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ValueError("Invalid native binary SHA-256")
    if len(application_ini) > 32 * 1024:
        raise ValueError("Runtime application.ini exceeds the size limit")
    ini = configparser.ConfigParser(interpolation=None, strict=True)
    ini.optionxform = str
    try:
        ini.read_string(application_ini.decode("utf-8"))
        # Match canonical App-local, case-sensitive identity extraction.
        ini.defaults().clear()
        identity = (ini["App"]["Version"], ini["App"]["BuildID"])
    except (UnicodeError, configparser.Error, KeyError) as error:
        raise ValueError("Invalid Runtime application.ini identity") from error
    if identity != (version, expected_build_id):
        raise ValueError("Runtime application.ini differs from the packing provenance")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--record", type=Path, required=True)
    parser.add_argument("--pin", type=Path, required=True)
    parser.add_argument("--runtime-head-sha", required=True)
    parser.add_argument("--expected-build-id", required=True)
    parser.add_argument("--application-ini", type=Path, required=True)
    args = parser.parse_args()
    try:
        validate_packaging_provenance(
            read_json(args.record), read_json(args.pin), args.runtime_head_sha,
            args.expected_build_id, read_regular(args.application_ini),
        )
    except (OSError, ValueError) as error:
        parser.exit(1, f"Runtime packaging provenance verification failed: {error}\n")
    print(f"Verified unchanged macOS Runtime packaging: {args.runtime_head_sha}")


if __name__ == "__main__":
    main()
