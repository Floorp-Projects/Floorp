# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import add_suffix
from taskgraph import MAX_DEPENDENCIES


transforms = TransformSequence()


def build_task_definition(orig_task, deps, count):
    task = deepcopy(orig_task)
    task['dependencies'] = deps
    task['name'] = "{}-{}".format(orig_task['name'], count)
    if 'treeherder' in task:
        task['treeherder']['symbol'] = add_suffix(
            task['treeherder']['symbol'], "-{}".format(count))

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
        dep_labels = sorted(task.pop('soft-dependencies', []))

        for dep_label in dep_labels:
            deps[dep_label] = dep_label
            if len(deps) == MAX_DEPENDENCIES:
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
        yield task
