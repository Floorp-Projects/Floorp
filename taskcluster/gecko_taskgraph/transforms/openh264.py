# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This transform is used to help populate mozharness options for openh264 jobs
"""

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def set_mh_options(config, jobs):
    """
    This transform sets the 'openh264_rev' attribute.
    """
    for job in jobs:
        repo = job.pop("repo")
        rev = job.pop("revision")
        attributes = job.setdefault("attributes", {})
        attributes["openh264_rev"] = rev
        run = job.setdefault("run", {})
        options = run.setdefault("options", [])
        options.extend([f"repo={repo}", f"rev={rev}"])
        yield job
