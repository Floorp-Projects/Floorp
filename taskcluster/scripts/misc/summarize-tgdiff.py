# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import json
import pathlib


def filter_changes(line):
    # Skip diff headers
    if line.startswith("---") or line.startswith("+++"):
        return False

    # Only count lines that changed
    return line.startswith("-") or line.startswith("+")


def run():
    parser = argparse.ArgumentParser(
        description="Classify output of taskgraph for CI analsyis"
    )
    parser.add_argument(
        "path",
        type=pathlib.Path,
        help="Folder containing all the TXT files from taskgraph target.",
    )
    parser.add_argument(
        "threshold",
        type=int,
        help="Minimum number of lines to trigger a warning on taskgraph output.",
    )
    args = parser.parse_args()

    out = {"files": {}, "status": "OK", "threshold": args.threshold}
    for path in args.path.glob("*.txt"):
        with path.open() as f:
            nb = len(list(filter(filter_changes, f.readlines())))

        out["files"][path.stem] = {
            "nb": nb,
            "status": "WARNING" if nb >= args.threshold else "OK",
        }

        if nb >= args.threshold:
            out["status"] = "WARNING"

    (args.path / "summary.json").write_text(json.dumps(out, sort_keys=True, indent=4))


if __name__ == "__main__":
    run()
