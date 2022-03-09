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

import sys
import subprocess

overall_failure = False

print("Run `hg status devtools/`")
status = (
    subprocess.check_output(["hg", "status", "devtools/"]).decode("utf-8").split("\n")
)
print(" status:")
print("-" * 80)
for l in status:
    if not l:
        # Ignore empty lines
        continue

    if "module-manifest.json" in l:
        # Ignore module-manifest.json updates which can randomly happen when
        # building bundles.
        continue

    print(f"TEST-UNEXPECTED-FAIL | devtools-bundle | {l}")
    overall_failure = True

# Revert all the changes created by `node bin/bundle.js`
subprocess.check_output(["hg", "revert", "-C", "."])

if overall_failure:
    sys.exit(1)
