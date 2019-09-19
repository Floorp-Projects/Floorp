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

# TODO remove this transform once build tasks are migrated to taskgraph
@transforms.add
def fetch_old_decision_dependency(config, tasks):
    for task in tasks:
        for dep_task in config.kind_dependencies_tasks:
            expected_signing_dep_label = task['dependencies']['build']
            if dep_task.label != expected_signing_dep_label:
                continue

            task['primary-dependency'] = dep_task
            yield task


@transforms.add
def define_signing_flags(config, tasks):
    for task in tasks:
        dep = task["primary-dependency"]
        # Current kind will be prepended later in the transform chain.
        task.setdefault("attributes", {}).update(dep.attributes.copy())
        task["attributes"]["signed"] = True
        if "run_on_tasks_for" in task["attributes"]:
            task["run-on-tasks-for"] = task["attributes"]["run_on_tasks_for"]

        for key in ("worker-type", "worker.signing-type"):
            resolve_keyed_by(
                task,
                key,
                item_name=task["name"],
                variant=task["attributes"]["build-type"],
                level=config.params["level"],
            )
        yield task


@transforms.add
def build_signing_task(config, tasks):
    for task in tasks:
        dep = task.pop("primary-dependency")
        task["dependencies"] = {"build": dep.label}
        task["worker"]["upstream-artifacts"] = [
            {
                "taskId": {"task-reference": "<build>"},
                "taskType": "build",
                "paths": dep.attributes["apks"].values(),
                "formats": ["autograph_apk"],
            }
        ]
        yield task
