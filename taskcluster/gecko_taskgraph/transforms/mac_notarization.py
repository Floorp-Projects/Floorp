# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform mac notarization tasks
"""

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def only_level_3_notarization(config, jobs):
    """Filter out any notarization jobs that are not level 3"""
    for job in jobs:
        if "notarization" in config.kind and int(config.params["level"]) != 3:
            continue
        yield job
