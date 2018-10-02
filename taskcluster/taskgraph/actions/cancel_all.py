# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import concurrent.futures as futures
import logging
import os

from taskgraph.util.taskcluster import list_task_group, cancel_task
from .registry import register_callback_action

# the maximum number of parallel cancelTask calls to make
CONCURRENCY = 50

logger = logging.getLogger(__name__)


@register_callback_action(
    title='Cancel All',
    name='cancel-all',
    kind='hook',
    generic=True,
    symbol='cAll',
    description=(
        'Cancel all running and pending tasks created by the decision task '
        'this action task is associated with.'
    ),
    order=400,
    context=[]
)
def cancel_all_action(parameters, graph_config, input, task_group_id, task_id, task):
    own_task_id = os.environ.get('TASK_ID', '')
    with futures.ThreadPoolExecutor(CONCURRENCY) as e:
        cancels_jobs = [
            e.submit(cancel_task, t, use_proxy=True)
            for t in list_task_group(task_group_id) if t != own_task_id
        ]
        for job in cancels_jobs:
            job.result()
