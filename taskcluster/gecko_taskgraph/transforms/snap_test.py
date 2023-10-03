# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
"""


import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.treeherder import inherit_treeherder_from_dep

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def fill_template(config, tasks):
    for task in tasks:
        dep = get_primary_dependency(config, task)
        assert dep

        inherit_treeherder_from_dep(task, dep)
        task_platform = task["task"]["extra"]["treeherder"]["machine"]["platform"]

        # Disambiguate the treeherder symbol.
        full_platform_collection = (
            task_platform + "-snap-" + task.get("label").split("-")[-1]
        )
        (platform, collection) = full_platform_collection.split("/")
        task["task"]["extra"]["treeherder"]["collection"] = {collection: True}
        task["task"]["extra"]["treeherder"]["machine"]["platform"] = platform
        task["task"]["extra"]["treeherder-platform"] = full_platform_collection

        timeout = 10
        if collection != "opt":
            timeout = 60
        task["task"]["payload"]["env"]["TEST_TIMEOUT"] = "{}".format(timeout)

        yield task
