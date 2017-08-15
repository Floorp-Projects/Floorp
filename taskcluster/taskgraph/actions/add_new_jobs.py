# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .registry import register_callback_action

from .util import (create_tasks, find_decision_task)
from taskgraph.util.taskcluster import get_artifact
from taskgraph.taskgraph import TaskGraph


@register_callback_action(
    name='add-new-jobs',
    title='Add new jobs',
    symbol='add-new',
    description="Add new jobs using task labels.",
    order=10000,
    context=[],
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

    to_run = []
    for elem in input['tasks']:
        if elem in full_task_graph.tasks:
            to_run.append(elem)
        else:
            raise Exception('{} was not found in the task-graph'.format(elem))

    create_tasks(to_run, full_task_graph, label_to_taskid, parameters, decision_task_id)
