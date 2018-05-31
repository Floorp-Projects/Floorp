# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging

from .util import (
    create_tasks,
    fetch_graph_and_labels
)
from .registry import register_callback_action

logger = logging.getLogger(__name__)


@register_callback_action(
    title='Retrigger',
    name='retrigger',
    symbol='rt',
    kind='hook',
    generic=True,
    description=(
        'Create a clone of the task.\n\n'
    ),
    order=1,
    context=[{}],
    schema={
        'type': 'object',
        'properties': {
            'downstream': {
                'type': 'boolean',
                'description': (
                    'If true, downstream tasks from this one will be cloned as well. '
                    'The dependencies will be updated to work with the new task at the root.'
                ),
                'default': False,
            },
            'times': {
                'type': 'integer',
                'default': 1,
                'minimum': 1,
                'maximum': 6,
                'title': 'Times',
                'description': 'How many times to run each task.',
            }
        }
    }
)
def retrigger_action(parameters, graph_config, input, task_group_id, task_id, task):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config)

    label = task['metadata']['name']
    with_downstream = ' '
    to_run = [label]

    if input.get('downstream'):
        to_run = full_task_graph.graph.transitive_closure(set(to_run), reverse=True).nodes
        to_run = to_run & set(label_to_taskid.keys())
        with_downstream = ' (with downstream) '

    times = input.get('times', 1)
    for i in xrange(times):
        create_tasks(to_run, full_task_graph, label_to_taskid, parameters, decision_task_id, i)

        logger.info('Scheduled {}{}(time {}/{})'.format(label, with_downstream, i+1, times))
