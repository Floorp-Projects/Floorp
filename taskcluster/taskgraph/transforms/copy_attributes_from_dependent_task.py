# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job

transforms = TransformSequence()


@transforms.add
def copy_attributes(config, jobs):
    for job in jobs:
        job.setdefault('attributes', {})
        job['attributes'].update(copy_attributes_from_dependent_job(job['dependent-task']))

        yield job
