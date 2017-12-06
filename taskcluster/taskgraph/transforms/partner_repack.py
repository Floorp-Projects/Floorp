# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the partner repack task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by


transforms = TransformSequence()


@transforms.add
def resolve_properties(config, tasks):
    for task in tasks:
        for property in ("repack_manifests_url", ):
            property = "worker.properties.{}".format(property)
            resolve_keyed_by(task, property, property, **config.params)
            yield task
