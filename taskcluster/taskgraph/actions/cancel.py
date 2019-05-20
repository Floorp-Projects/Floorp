# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import requests

from taskgraph.util.taskcluster import cancel_task
from .registry import register_callback_action

logger = logging.getLogger(__name__)


@register_callback_action(
    title='Cancel Task',
    name='cancel',
    symbol='cx',
    generic=True,
    description=(
        'Cancel the given task'
    ),
    order=350,
    context=[{}]
)
def cancel_action(parameters, graph_config, input, task_group_id, task_id):
    # Note that this is limited by the scopes afforded to generic actions to
    # only cancel tasks with the level-specific schedulerId.
    try:
        cancel_task(task_id, use_proxy=True)
    except requests.HTTPError as e:
        if e.response.status_code == 409:
            # A 409 response indicates that this task is past its deadline.  It
            # cannot be cancelled at this time, but it's also not running
            # anymore, so we can ignore this error.
            logger.info('Task is past its deadline and cannot be cancelled.'.format(task_id))
            return
        raise
