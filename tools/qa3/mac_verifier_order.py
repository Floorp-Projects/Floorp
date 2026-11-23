# SPDX-License-Identifier: MPL-2.0

"""Check the Firefox45 stage order in an authenticated Mac run summary."""

import argparse
from pathlib import Path

from source import read_json

REQUIRED_STAGES = (
    "configure",
    "effective-config",
    "fresh45-exports",
    "mozilla-upstream-helpers-build",
    "mozilla-upstream-helpers-install",
    "mozilla-upstream-inventory",
    "mozilla-shell-build",
    "mozilla-http3-build",
    "mozilla-browser-build",
    "mozilla-updater-build",
    "mozilla-channel-build",
    "mozilla-dmg-build",
    "mozilla-resources-build",
    "mozilla-upstream-child-build",
    "mozilla-upstream-child-libs",
    "mozilla-upstream-resources-build",
)
FORBIDDEN_STAGES = {
    "full-build",
    "stage-package",
    "native-suite",
    "adhoc-copy",
    "verify-copy",
    "verify-copy-after",
}


def validate_stage_order(summary):
    stages = summary.get("stages") if isinstance(summary, dict) else None
    if not isinstance(stages, list) or not 0 < len(stages) <= 128:
        raise ValueError("expected 1 to 128 stage records")

    required_stages = []
    for row in stages:
        stage = row.get("stage") if isinstance(row, dict) else None
        if not isinstance(stage, str):
            raise ValueError("stage records must have a string name")
        if stage in FORBIDDEN_STAGES or stage.startswith("checkpoint-"):
            raise ValueError("stage is outside the Firefox45 verifier scope")
        if stage in REQUIRED_STAGES:
            required_stages.append(stage)

    if tuple(required_stages) != REQUIRED_STAGES:
        raise ValueError(
            "required Firefox45 stages must appear exactly once and in order, "
            "with inventory after helper installation and before target builds"
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("summary", type=Path)
    args = parser.parse_args()
    try:
        validate_stage_order(read_json(args.summary, max_bytes=16 * 1024 * 1024))
    except (OSError, ValueError, RecursionError) as error:
        parser.exit(1, f"Mac verifier order check failed: {error}\n")
    print("Mac Firefox45 stage order verified")


if __name__ == "__main__":
    main()
