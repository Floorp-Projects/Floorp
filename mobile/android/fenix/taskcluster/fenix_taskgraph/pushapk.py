# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import inherit_treeherder_from_dep


transforms = TransformSequence()


# TODO remove this transform once signing task are migrated to taskgraph
@transforms.add
def fetch_old_decision_dependency(config, tasks):
    for task in tasks:
        for dep_task in config.kind_dependencies_tasks:
            expected_signing_dep_label = task['dependencies']['signing']
            if dep_task.label != expected_signing_dep_label:
                continue

            task['primary-dependency'] = dep_task
            yield task


@transforms.add
def build_pushapk_task(config, tasks):
    for task in tasks:
        dep = task.pop("primary-dependency")
        task.setdefault("attributes", {}).update(dep.attributes.copy())
        if "run_on_tasks_for" in task["attributes"]:
            task["run-on-tasks-for"] = task["attributes"]["run_on_tasks_for"]

        task["treeherder"] = inherit_treeherder_from_dep(task, dep)
        task["worker"]["upstream-artifacts"] = [{
            "taskId": {"task-reference": "<signing>"},
            "taskType": "signing",
            "paths": dep.attributes["apks"].values(),
        }]
        task["worker"]["dep"] = config.params["level"] != "3"
        if not task["worker"].get("certificate-alias"):
            task["worker"]["certificate-alias"] = "{}-{}".format(
                task["worker"]["product"], task["worker"]["channel"]
            )

        yield task
