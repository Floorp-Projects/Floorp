# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .registry import register_callback_action
from slugid import nice as slugid

from .util import (create_task, find_decision_task)
from taskgraph.util.taskcluster import get_artifact
from taskgraph.util.parameterization import resolve_task_references
from taskgraph.taskgraph import TaskGraph


@register_callback_action(
    name='add-new-jobs',
    title='Add new jobs',
    symbol='add-new',
    description="Add new jobs using task labels",
    order=10000,
    context=[{}],
    schema={
        'type': 'object',
        'properties': {
            'tasks': {
                'type': 'array',
                'description': 'An array of task labels',
                'items': {
                    'type': 'string'
                }
            }
        }
    }
)
def add_new_jobs_action(parameters, input, task_group_id, task_id, task):
    decision_task_id = find_decision_task(parameters)

    full_task_graph = get_artifact(decision_task_id, "public/full-task-graph.json")
    _, full_task_graph = TaskGraph.from_json(full_task_graph)
    label_to_taskid = get_artifact(decision_task_id, "public/label-to-taskid.json")

    for elem in input['tasks']:
        if elem in full_task_graph.tasks:
            task = full_task_graph.tasks[elem]

            # fix up the task's dependencies, similar to how optimization would
            # have done in the decision
            dependencies = {name: label_to_taskid[label]
                            for name, label in task.dependencies.iteritems()}
            task_def = resolve_task_references(task.label, task.task, dependencies)
            task_def.setdefault('dependencies', []).extend(dependencies.itervalues())
            # actually create the new task
            create_task(slugid(), task_def, parameters['level'])
        else:
            raise Exception('{} was not found in the task-graph'.format(elem))
