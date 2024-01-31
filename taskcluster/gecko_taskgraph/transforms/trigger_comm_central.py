#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""
Resolve keys for the jobs defined in the trigger-comm-central kind.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job,
            "scopes",
            item_name=job["name"],
            level=config.params["level"],
        )
        yield job
