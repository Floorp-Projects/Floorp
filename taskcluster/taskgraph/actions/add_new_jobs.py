# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .registry import register_callback_action

from .util import (create_tasks, fetch_graph_and_labels)


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
def add_new_jobs_action(parameters, graph_config, input, task_group_id, task_id, task):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config)

    to_run = []
    for elem in input['tasks']:
        if elem in full_task_graph.tasks:
            to_run.append(elem)
        else:
            raise Exception('{} was not found in the task-graph'.format(elem))

    create_tasks(to_run, full_task_graph, label_to_taskid, parameters, decision_task_id)
