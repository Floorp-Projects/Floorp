# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from copy import deepcopy

from taskgraph import MAX_DEPENDENCIES
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import add_suffix

# XXX Docker images may be added after this transform, so we allow one more dep to be added
MAX_NUMBER_OF_DEPS = MAX_DEPENDENCIES - 1

transforms = TransformSequence()


def build_task_definition(orig_task, deps, soft_deps, count):
    task = deepcopy(orig_task)
    task["dependencies"] = {label: label for label in deps}
    task["soft-dependencies"] = list(soft_deps)
    task["name"] = "{}-{}".format(orig_task["name"], count)
    if "treeherder" in task:
        task["treeherder"]["symbol"] = add_suffix(
            task["treeherder"]["symbol"], f"-{count}"
        )

    task["attributes"]["is_final_chunked_task"] = False
    return task


def get_chunked_label(config, chunked_task):
    return "{}-{}".format(config.kind, chunked_task["name"])


@transforms.add
def add_dependencies(config, tasks):
    for task in tasks:
        count = 1
        soft_deps = set()
        regular_deps = set()
        chunked_labels = set()

        soft_dep_labels = list(task.pop("soft-dependencies", []))
        regular_dep_labels = list(task.get("dependencies", {}).keys())
        # sort for deterministic chunking
        all_dep_labels = sorted(set(soft_dep_labels + regular_dep_labels))

        for dep_label in all_dep_labels:
            if dep_label in regular_dep_labels:
                regular_deps.add(dep_label)
            else:
                soft_deps.add(dep_label)

            if len(regular_deps) + len(soft_deps) == MAX_NUMBER_OF_DEPS:
                chunked_task = build_task_definition(
                    task, regular_deps, soft_deps, count
                )
                chunked_label = get_chunked_label(config, chunked_task)
                chunked_labels.add(chunked_label)
                yield chunked_task
                soft_deps.clear()
                regular_deps.clear()
                count += 1

        if regular_deps or soft_deps:
            chunked_task = build_task_definition(task, regular_deps, soft_deps, count)
            chunked_label = get_chunked_label(config, chunked_task)
            chunked_labels.add(chunked_label)
            yield chunked_task

        task["dependencies"] = {label: label for label in chunked_labels}
        # Chunk yields a last task that doesn't have a number appended to it.
        # It helps configuring Github which waits on a single label.
        # Setting this attribute also enables multi_dep to select the right
        # task to depend on.
        task["attributes"]["is_final_chunked_task"] = True
        yield task
