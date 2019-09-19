# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the nimbledroid
kind. This transform won't be necessary anymore once the full tasgraph migration is
done
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy
import json

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import inherit_treeherder_from_dep
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def filter_out_non_nightly_legacy(config, tasks):
    for dep_task in config.kind_dependencies_tasks:
        if dep_task.label == 'Build FenixNightlyLegacy task':
            for task in tasks:
                yield task
