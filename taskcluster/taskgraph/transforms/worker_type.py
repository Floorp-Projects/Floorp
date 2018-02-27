# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Resolve worker-type by project
"""
from __future__ import absolute_import, print_function, unicode_literals
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def resolve_worker_type(config, jobs):
    for job in jobs:
        resolve_keyed_by(job, 'worker-type', job['description'], **config.params)
        yield job
