# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        for key in ["worker-type", "scopes"]:
            resolve_keyed_by(
                job,
                key,
                item_name=job["name"],
                **{"release-level": config.params.release_level()}
            )
        yield job
