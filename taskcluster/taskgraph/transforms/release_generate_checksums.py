
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the checksums task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals
import copy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.scriptworker import get_release_config
from taskgraph.util.schema import (
    resolve_keyed_by,
)

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def handle_keyed_by(config, jobs):
    """Resolve fields that can be keyed by project, etc."""
    fields = [
        "run.config",
        "run.extra-config",
    ]
    for job in jobs:
        job = copy.deepcopy(job)
        for field in fields:
            resolve_keyed_by(
                item=job,
                field=field,
                item_name=job['name'],
                project=config.params['project']
            )
        yield job


@transforms.add
def interpolate(config, jobs):
    release_config = get_release_config(config)
    for job in jobs:
        mh_options = list(job["run"]["options"])
        job["run"]["options"] = [
            option.format(version=release_config["version"],
                          build_number=release_config["build_number"])
            for option in mh_options
        ]
        yield job
