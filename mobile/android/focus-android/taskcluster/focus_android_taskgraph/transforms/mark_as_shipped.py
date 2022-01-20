# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by


transforms = TransformSequence()


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        resolve_keyed_by(
            task,
            'scopes',
            item_name=task["name"],
            **{
                'level': config.params["level"],
            }
        )
        yield task


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        # used to query shipit so the case needs to match
        product = "Focus-android"
        version = config.params['version'] or "{ver}"
        job['worker']['release-name'] = '{product}-{version}-build{build_number}'.format(
            product=product,
            version=version,
            build_number=config.params.get('build_number', 1)
        )
        yield job
