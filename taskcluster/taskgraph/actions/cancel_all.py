# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import concurrent.futures as futures
import logging
import os

from taskgraph.util.taskcluster import get_session, cancel_task
from .registry import register_callback_action

# the maximum number of parallel cancelTask calls to make
CONCURRENCY = 50

base_url = 'https://queue.taskcluster.net/v1/{}'

logger = logging.getLogger(__name__)


def list_group(task_group_id, session):
    params = {}
    while True:
        url = base_url.format('task-group/{}/list'.format(task_group_id))
        response = session.get(url, stream=True, params=params)
        response.raise_for_status()
        response = response.json()
        for task in [t['status'] for t in response['tasks']]:
            if task['state'] in ['running', 'pending', 'unscheduled']:
                yield task['taskId']
        if response.get('continuationToken'):
            params = {'continuationToken': response.get('continuationToken')}
        else:
            break


@register_callback_action(
    title='Cancel All',
    name='cancel-all',
    symbol='cAll',
    description=(
        'Cancel all running and pending tasks created by the decision task '
        'this action task is associated with.'
    ),
    order=100,
    context=[]
)
def cancel_all_action(parameters, graph_config, input, task_group_id, task_id, task):
    session = get_session()
    own_task_id = os.environ.get('TASK_ID', '')
    with futures.ThreadPoolExecutor(CONCURRENCY) as e:
        cancels_jobs = [
            e.submit(cancel_task, t, use_proxy=True)
            for t in list_group(task_group_id, session) if t != own_task_id
        ]
        for job in cancels_jobs:
            job.result()
