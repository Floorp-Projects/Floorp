#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0

import argparse
import hashlib
import json
import re
from pathlib import Path


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Source provenance contains a duplicate JSON field")
        result[key] = value
    return result


def read_regular(path: Path) -> bytes:
    if path.is_symlink() or not path.is_file():
        raise ValueError("Source provenance input must be a regular, unlinked file")
    return path.read_bytes()


def read_json(path: Path):
    data = read_regular(path)
    if len(data) > 128 * 1024:
        raise ValueError("Source provenance JSON exceeds the size limit")
    return json.loads(data, object_pairs_hook=unique_object)


def closed_object(value, keys, description):
    if not isinstance(value, dict) or set(value) != set(keys):
        raise ValueError(f"Invalid {description} schema")


def validate_upstream_pin(upstream):
    legacy_keys = ("repository", "version", "tag", "commit", "previous_tag", "previous_commit")
    snapshot_keys = (*legacy_keys, "tracking_ref", "tree")
    if isinstance(upstream, dict) and set(upstream) == set(snapshot_keys):
        # A release-branch snapshot is identified by its immutable commit/tree.
        # The ref only describes where that snapshot was selected; never resolve it.
        if upstream["tracking_ref"] != "refs/heads/release":
            raise ValueError("Invalid upstream snapshot tracking ref")
        for field in ("commit", "tree", "previous_commit"):
            if not isinstance(upstream[field], str) or not re.fullmatch(
                    r"[0-9a-f]{40}", upstream[field]):
                raise ValueError(f"Invalid upstream snapshot {field}")
        version = upstream["version"]
        if not isinstance(version, str) or not re.fullmatch(r"[0-9]+\.[0-9]+(?:\.[0-9]+)?", version):
            raise ValueError("Invalid upstream snapshot version")
        if upstream["tag"] not in (None, f"FIREFOX_{version.replace('.', '_')}_RELEASE"):
            raise ValueError("Invalid upstream snapshot tag")
    else:
        closed_object(upstream, legacy_keys, "upstream pin")


def validate_source_provenance(record, pin, runtime_head_sha, floorp_patch: bytes):
    if not re.fullmatch(r"[0-9a-f]{40}", runtime_head_sha):
        raise ValueError("Expected Runtime commit is not a canonical SHA")
    closed_object(pin, ("schema_version", "upstream", "floorp_patch"), "Runtime pin")
    closed_object(record, ("schema_version", "runtime_commit", "upstream", "floorp_patch"),
                  "compiled Runtime source provenance")
    if type(pin["schema_version"]) is not int or pin["schema_version"] != 1:
        raise ValueError("Unsupported Runtime source pin version")
    if type(record["schema_version"]) is not int or record["schema_version"] != 1:
        raise ValueError("Unsupported Runtime source provenance version")
    validate_upstream_pin(pin["upstream"])
    closed_object(pin["floorp_patch"],
                  ("repository", "commit", "path", "runtime_path", "sha256", "patched_files"),
                  "Floorp patch pin")
    if pin["upstream"]["repository"] != "mozilla-firefox/firefox":
        raise ValueError("Unexpected upstream source repository")
    if pin["floorp_patch"]["repository"] != "Floorp-Projects/Floorp":
        raise ValueError("Unexpected Floorp patch repository")
    if record["runtime_commit"] != runtime_head_sha:
        raise ValueError("Compiled Runtime source commit differs from the authenticated run")
    if record["upstream"] != pin["upstream"]:
        raise ValueError("Compiled upstream source differs from the authenticated Runtime pin")
    if record["floorp_patch"] != pin["floorp_patch"]:
        raise ValueError("Compiled Floorp patch differs from the authenticated Runtime pin")
    if hashlib.sha256(floorp_patch).hexdigest() != pin["floorp_patch"]["sha256"]:
        raise ValueError("Compiled Floorp patch differs from the current Floorp patch")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--record", type=Path, required=True)
    parser.add_argument("--pin", type=Path, required=True)
    parser.add_argument("--runtime-head-sha", required=True)
    parser.add_argument("--floorp-patch", type=Path, required=True)
    args = parser.parse_args()
    try:
        pin = read_json(args.pin)
        validate_source_provenance(read_json(args.record), pin, args.runtime_head_sha,
                                   read_regular(args.floorp_patch))
    except (OSError, ValueError) as error:
        parser.exit(1, f"Runtime source provenance verification failed: {error}\n")
    print(f"Verified compiled Runtime source provenance: {args.runtime_head_sha}")


if __name__ == "__main__":
    main()
