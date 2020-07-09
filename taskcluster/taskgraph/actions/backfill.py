# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging

import requests
from requests.exceptions import HTTPError

from .registry import register_callback_action
from .util import create_tasks, combine_task_graph_files
from taskgraph.util.taskcluster import get_artifact_from_index
from taskgraph.util.taskgraph import find_decision_task
from taskgraph.taskgraph import TaskGraph
from taskgraph.util import taskcluster

PUSHLOG_TMPL = '{}/json-pushes?version=2&startID={}&endID={}'
INDEX_TMPL = 'gecko.v2.{}.pushlog-id.{}.decision'

logger = logging.getLogger(__name__)


@register_callback_action(
    title='Backfill',
    name='backfill',
    symbol='Bk',
    description=('Take the label of the current task, '
                 'and trigger the task with that label '
                 'on previous pushes in the same project.'),
    order=200,
    context=[{}],  # This will be available for all tasks
    schema={
        'type': 'object',
        'properties': {
            'depth': {
                'type': 'integer',
                'default': 9,
                'minimum': 1,
                'maximum': 25,
                'title': 'Depth',
                'description': ('The number of previous pushes before the current '
                                'push to attempt to trigger this task on.')
            },
            'inclusive': {
                'type': 'boolean',
                'default': False,
                'title': 'Inclusive Range',
                'description': ('If true, the backfill will also retrigger the task '
                                'on the selected push.')
            },
            'times': {
                'type': 'integer',
                'default': 1,
                'minimum': 1,
                'maximum': 10,
                'title': 'Times',
                'description': ('The number of times to execute each job '
                                'you are backfilling.')
            }
        },
        'additionalProperties': False
    },
    available=lambda parameters: True
)
def backfill_action(parameters, graph_config, input, task_group_id, task_id):
    task = taskcluster.get_task_definition(task_id)
    label = task['metadata']['name']
    pushes = []
    inclusive_tweak = 1 if input.get('inclusive') else 0
    depth = input.get('depth', 9) + inclusive_tweak
    end_id = int(parameters['pushlog_id']) - (1 - inclusive_tweak)

    while True:
        start_id = max(end_id - depth, 0)
        pushlog_url = PUSHLOG_TMPL.format(parameters['head_repository'], start_id, end_id)
        r = requests.get(pushlog_url)
        r.raise_for_status()
        pushes = pushes + r.json()['pushes'].keys()
        if len(pushes) >= depth:
            break

        end_id = start_id - 1
        start_id -= depth
        if start_id < 0:
            break

    pushes = sorted(pushes)[-depth:]
    backfill_pushes = []

    for push in pushes:
        try:
            full_task_graph = get_artifact_from_index(
                    INDEX_TMPL.format(parameters['project'], push),
                    'public/full-task-graph.json')
            _, full_task_graph = TaskGraph.from_json(full_task_graph)
            label_to_taskid = get_artifact_from_index(
                    INDEX_TMPL.format(parameters['project'], push),
                    'public/label-to-taskid.json')
            push_params = get_artifact_from_index(
                    INDEX_TMPL.format(parameters['project'], push),
                    'public/parameters.yml')
            push_decision_task_id = find_decision_task(push_params, graph_config)
        except HTTPError as e:
            logger.info('Skipping {} due to missing index artifacts! Error: {}'.format(push, e))
            continue

        if label in full_task_graph.tasks.keys():
            times = input.get('times', 1)
            for i in range(times):
                create_tasks(graph_config, [label], full_task_graph, label_to_taskid,
                             push_params, push_decision_task_id, push)
            backfill_pushes.append(push)
        else:
            logging.info('Could not find {} on {}. Skipping.'.format(label, push))
    combine_task_graph_files(backfill_pushes)
