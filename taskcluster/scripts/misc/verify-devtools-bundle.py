#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Check that the current sourcemap and worker bundles built for DevTools are up to date.
This job should fail if any file impacting the bundle creation was modified without
regenerating the bundles.

This check should be run after building the bundles via:
cd devtools/client/debugger
yarn && node bin/bundle.js

Those steps are done in the devtools-verify-bundle job, prior to calling this script.
The script will only run `hg status devtools/` and check that no change is detected by
mercurial.
"""

import argparse
import json
import subprocess
import sys

# Ignore module-manifest.json updates which can randomly happen when
# building bundles.
hg_exclude = "devtools/client/debugger/bin/module-manifest.json"

print("Run `hg status devtools/`")
status = (
    subprocess.check_output(["hg", "status", "-n", "devtools/", "-X", hg_exclude])
    .decode("utf-8")
    .split("\n")
)
print(" status:")
print("-" * 80)

doc = "https://firefox-source-docs.mozilla.org/devtools/tests/node-tests.html#devtools-bundle"

failures = {}
for l in status:
    if not l:
        # Ignore empty lines
        continue

    failures[l] = [
        {
            "path": l,
            "line": None,
            "column": None,
            "level": "error",
            "message": l
            + " is outdated and needs to be regenerated, "
            + f"instructions at: {doc}",
        }
    ]


diff = subprocess.check_output(["hg", "diff", "devtools/", "-X", hg_exclude]).decode(
    "utf-8"
)

# Revert all the changes created by `node bin/bundle.js`
subprocess.check_output(["hg", "revert", "-C", "devtools/"])

parser = argparse.ArgumentParser()
parser.add_argument("--output", required=True)
args = parser.parse_args()

with open(args.output, "w") as fp:
    json.dump(failures, fp, indent=2)

if len(failures) > 0:
    print(
        "TEST-UNEXPECTED-FAIL | devtools-bundle | DevTools bundles need to be regenerated, "
        + f"instructions at: {doc}"
    )

    print("The following devtools bundles were detected as outdated:")
    for failure in failures:
        print(failure)

    print(f"diff:{diff}")

    sys.exit(1)
