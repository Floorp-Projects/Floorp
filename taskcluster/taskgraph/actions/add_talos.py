# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging

from .registry import register_callback_action
from .util import create_tasks, fetch_graph_and_labels

logger = logging.getLogger(__name__)


@register_callback_action(
    name='run-all-talos',
    title='Run All Talos Tests',
    kind='hook',
    generic=True,
    symbol='raT',
    description="Add all Talos tasks to a push.",
    order=100,  # Useful for sheriffs, but not top of the list
    context=[],
    schema={
        'type': 'object',
        'properties': {
            'times': {
                'type': 'integer',
                'default': 1,
                'minimum': 1,
                'maximum': 6,
                'title': 'Times',
                'description': 'How many times to run each task.',
            }
        },
        'additionalProperties': False
    },
)
def add_all_talos(parameters, graph_config, input, task_group_id, task_id, task):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config)

    times = input.get('times', 1)
    for i in xrange(times):
        to_run = [label
                  for label, entry
                  in full_task_graph.tasks.iteritems() if 'talos_try_name' in entry.attributes]

        create_tasks(to_run, full_task_graph, label_to_taskid, parameters, decision_task_id)
        logger.info('Scheduled {} talos tasks (time {}/{})'.format(len(to_run), i+1, times))
