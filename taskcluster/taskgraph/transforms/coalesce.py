# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This transform enables coalescing for tasks with scm level > 1, by defining
values for the coalesce settings of the job.
"""

from __future__ import absolute_import

from taskgraph.transforms.base import TransformSequence
from hashlib import sha256

transforms = TransformSequence()


@transforms.add
def enable_coalescing(config, jobs):
    """
    This transform enables coalescing for tasks with scm level > 1, setting the
    name to be equal to the the first 20 chars of the sha256 of the task name
    (label). The hashing is simply used to keep the name short, unique, and
    alphanumeric, since it is used in AMQP routing keys.

    Note, the coalescing keys themselves are not intended to be readable
    strings or embed information. The tasks they represent contain all relevant
    metadata.
    """
    for job in jobs:
        if int(config.params['level']) > 1:
            job['coalesce'] = {
                'job-identifier': sha256(job["label"]).hexdigest()[:20],
                'age': 3600,
                'size': 5,
            }
        yield job
