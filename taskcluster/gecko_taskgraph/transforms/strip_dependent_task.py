# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
FIXME
"""

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def strip_dependent_task(config, jobs):
    for job in jobs:
        del job["primary-dependency"]
        yield job
