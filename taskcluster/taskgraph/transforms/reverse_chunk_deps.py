# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Adjust dependencies to not exceed MAX_DEPS
"""

from __future__ import absolute_import, print_function, unicode_literals
from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
import taskgraph.transforms.release_deps as release_deps
from taskgraph.util.treeherder import split_symbol, join_symbol

transforms = TransformSequence()

# Max dependency limit per task.
# https://docs.taskcluster.net/reference/platform/taskcluster-queue/references/api#createTask
MAX_DEPS = 100


def yield_job(orig_job, deps, count):
    job = deepcopy(orig_job)
    job['dependencies'] = deps
    job['name'] = "{}-{}".format(orig_job['name'], count)
    if 'treeherder' in job:
        groupSymbol, symbol = split_symbol(job['treeherder']['symbol'])
        symbol += '-'
        symbol += str(count)
        job['treeherder']['symbol'] = join_symbol(groupSymbol, symbol)

    return job


@transforms.add
def add_dependencies(config, jobs):
    for job in release_deps.add_dependencies(config, jobs):
        count = 1
        deps = {}

        for dep_label in job['dependencies'].keys():
            deps[dep_label] = dep_label
            if len(deps) == MAX_DEPS:
                yield yield_job(job, deps, count)
                deps = {}
                count += 1
        if deps:
            yield yield_job(job, deps, count)
            count += 1
