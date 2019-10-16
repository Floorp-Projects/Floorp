# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import add_suffix
from taskgraph import MAX_DEPENDENCIES


transforms = TransformSequence()


@transforms.add
def fill_dependencies(config, tasks):
    for task in tasks:
        dependencies = ('<{}>'.format(dep) for dep in task['dependencies'].keys())
        task['run']['command']['task-reference'] = task['run']['command']['task-reference'].format(
            dependencies=' '.join(dependencies)
        )

        yield task
