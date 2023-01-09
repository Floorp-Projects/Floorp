# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Adjust dependencies to not exceed MAX_DEPENDENCIES
"""

from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import add_suffix

import gecko_taskgraph.transforms.release_deps as release_deps
from gecko_taskgraph import MAX_DEPENDENCIES

transforms = TransformSequence()


def yield_job(orig_job, deps, count):
    job = deepcopy(orig_job)
    job["dependencies"] = deps
    job["name"] = "{}-{}".format(orig_job["name"], count)
    if "treeherder" in job:
        job["treeherder"]["symbol"] = add_suffix(
            job["treeherder"]["symbol"], f"-{count}"
        )

    return job


@transforms.add
def add_dependencies(config, jobs):
    for job in release_deps.add_dependencies(config, jobs):
        count = 1
        deps = {}

        # sort for deterministic chunking
        for dep_label in sorted(job["dependencies"].keys()):
            deps[dep_label] = dep_label
            if len(deps) == MAX_DEPENDENCIES:
                yield yield_job(job, deps, count)
                deps = {}
                count += 1
        if deps:
            yield yield_job(job, deps, count)
            count += 1
