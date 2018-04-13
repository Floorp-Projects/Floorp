# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging

import requests
from requests.exceptions import HTTPError

from .registry import register_callback_action
from .util import find_decision_task, create_tasks
from taskgraph.util.taskcluster import get_artifact_from_index
from taskgraph.taskgraph import TaskGraph

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
    order=0,
    context=[{}],  # This will be available for all tasks
    schema={
        'type': 'object',
        'properties': {
            'depth': {
                'type': 'integer',
                'default': 5,
                'minimum': 1,
                'maximum': 10,
                'title': 'Depth',
                'description': ('The number of previous pushes before the current '
                                'push to attempt to trigger this task on.')
            }
        },
        'additionalProperties': False
    },
    available=lambda parameters: parameters.get('project', None) != 'try'
)
def backfill_action(parameters, graph_config, input, task_group_id, task_id, task):
    label = task['metadata']['name']
    pushes = []
    depth = input.get('depth', 5)
    end_id = int(parameters['pushlog_id']) - 1

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
            create_tasks(
                    [label], full_task_graph, label_to_taskid,
                    push_params, push_decision_task_id, push)
        else:
            logging.info('Could not find {} on {}. Skipping.'.format(label, push))
