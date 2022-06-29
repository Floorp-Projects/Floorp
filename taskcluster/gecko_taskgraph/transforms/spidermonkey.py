# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import copy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def handle_keyed_by(config, jobs):
    """Resolve fields that can be keyed by platform, etc."""
    fields = ["fetches.toolchain"]
    for job in jobs:
        job = copy.deepcopy(job)  # don't overwrite dict values here
        for field in fields:
            resolve_keyed_by(item=job, field=field, item_name=job["name"])

        yield job
