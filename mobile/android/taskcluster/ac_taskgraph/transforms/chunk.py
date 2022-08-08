# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import add_suffix
from taskgraph import MAX_DEPENDENCIES

# XXX Docker images may be added after this transform, so we allow one more dep to be added
MAX_REGULAR_DEPS = MAX_DEPENDENCIES - 1

transforms = TransformSequence()


def build_task_definition(orig_task, deps, count):
    task = deepcopy(orig_task)
    task['dependencies'] = deps
    task['name'] = "{}-{}".format(orig_task['name'], count)
    if 'treeherder' in task:
        task['treeherder']['symbol'] = add_suffix(
            task['treeherder']['symbol'], f"-{count}")

    task["attributes"]["is_final_chunked_task"] = False
    return task


def get_chunked_label(config, chunked_task):
    return '{}-{}'.format(config.kind, chunked_task['name'])


@transforms.add
def add_dependencies(config, tasks):
    for task in tasks:
        count = 1
        deps = {}
        chunked_labels = {}

        # sort for deterministic chunking
        dep_labels = task.pop('soft-dependencies', [])
        dep_labels.extend(task.get('dependencies', {}).keys())
        dep_labels = sorted(dep_labels)

        for dep_label in dep_labels:
            deps[dep_label] = dep_label
            if len(deps) == MAX_REGULAR_DEPS:
                chunked_task = build_task_definition(task, deps, count)
                chunked_label = get_chunked_label(config, chunked_task)
                chunked_labels[chunked_label] = chunked_label
                yield chunked_task
                deps = {}
                count += 1
        if deps:
            chunked_task = build_task_definition(task, deps, count)
            chunked_label = get_chunked_label(config, chunked_task)
            chunked_labels[chunked_label] = chunked_label
            yield chunked_task
            count += 1

        task['dependencies'] = chunked_labels
        # Chunk yields a last task that doesn't have a number appended to it.
        # It helps configuring Github which waits on a single label.
        # Setting this attribute also enables multi_dep to select the right
        # task to depend on.
        task["attributes"]["is_final_chunked_task"] = True
        yield task
