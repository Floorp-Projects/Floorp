# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the checksums task into an actual task description.
"""

import copy

from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.util.attributes import release_level
from gecko_taskgraph.util.scriptworker import get_release_config

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
                item_name=job["name"],
                **{"release-level": release_level(config.params["project"])}
            )
        yield job


@transforms.add
def interpolate(config, jobs):
    release_config = get_release_config(config)
    for job in jobs:
        mh_options = list(job["run"]["options"])
        job["run"]["options"] = [
            option.format(
                version=release_config["version"],
                build_number=release_config["build_number"],
            )
            for option in mh_options
        ]
        yield job
