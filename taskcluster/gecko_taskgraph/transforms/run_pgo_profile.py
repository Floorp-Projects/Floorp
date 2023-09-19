# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the pgo jobs.
"""

import logging

from taskgraph.transforms.base import TransformSequence

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def run_profile_data(config, jobs):
    for job in jobs:
        build_platform = job["attributes"].get("build_platform")
        instr = "instrumented-build-{}".format(job["name"])
        if "android" in build_platform:
            artifact = "geckoview-test_runner.apk"
        elif "macosx64" in build_platform:
            artifact = "target.dmg"
        elif "win" in build_platform:
            artifact = "target.zip"
        else:
            artifact = "target.tar.bz2"
        job.setdefault("fetches", {})[instr] = [
            {"artifact": artifact, "extract": not artifact.endswith((".dmg", ".apk"))},
            "target.crashreporter-symbols.zip",
        ]
        yield job
