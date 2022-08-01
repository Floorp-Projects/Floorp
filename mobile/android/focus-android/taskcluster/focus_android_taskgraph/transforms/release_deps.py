# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add dependencies to release tasks.
"""

from taskgraph.transforms.base import TransformSequence

PHASES = ["build", "promote", "push", "ship"]

transforms = TransformSequence()


@transforms.add
def add_dependencies(config, tasks):
    for task in tasks:
        dependencies = {}
        # Add any kind_dependencies_tasks with matching release_type as dependencies
        release_type = task["attributes"].get("release-type")
        phase = task["attributes"].get("shipping_phase")
        if release_type is None:
            continue

        for dep_task in config.kind_dependencies_tasks:
            # Weed out unwanted tasks.
            # XXX we have run-on-projects which specifies the on-push behavior;
            # we need another attribute that specifies release promotion,
            # possibly which action(s) each task belongs in.

            # We can only depend on tasks in the current or previous phases
            dep_phase = dep_task.attributes.get("shipping_phase")
            if dep_phase and PHASES.index(dep_phase) > PHASES.index(phase):
                continue

            # Add matching release_type tasks to deps
            if (
                dep_task.task.get("release-type") == release_type
                or dep_task.attributes.get("release-type") == release_type
            ):
                dependencies[dep_task.label] = dep_task.label

        task.setdefault("dependencies", {}).update(dependencies)

        yield task
