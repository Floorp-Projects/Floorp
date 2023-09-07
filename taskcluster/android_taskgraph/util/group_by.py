# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.util.dependencies import group_by


@group_by("component")
def component_grouping(config, tasks):
    groups = {}
    for task in tasks:
        component = task.attributes.get("component")
        if component == "all":
            continue

        build_type = task.attributes.get("build-type")
        groups.setdefault((component, build_type), []).append(task)

    tasks_for_all_components = [
        task
        for task in tasks
        if task.attributes.get("component") == "all"
        # We just want to depend on the task that waits on all chunks. This way
        # we have a single dependency for that kind
        and task.attributes.get("is_final_chunked_task", True)
    ]
    for (_, build_type), tasks in groups.items():
        tasks.extend(
            [
                task
                for task in tasks_for_all_components
                if task.attributes.get("build-type") == build_type
            ]
        )

    return groups.values()


@group_by("build-type")
def build_type_grouping(config, tasks):
    groups = {}
    for task in tasks:
        # We just want to depend on the task that waits on all chunks. This way
        # we have a single dependency for that kind
        if not task.attributes.get("is_final_chunked_task", True):
            continue

        build_type = task.attributes.get("build-type")
        groups.setdefault(build_type, []).append(task)

    return groups.values()
