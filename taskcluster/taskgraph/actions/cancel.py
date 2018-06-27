# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.taskcluster import cancel_task
from .registry import register_callback_action


@register_callback_action(
    title='Cancel Task',
    name='cancel',
    symbol='cx',
    kind='hook',
    generic=True,
    description=(
        'Cancel the given task'
    ),
    order=100,
    context=[{}]
)
def cancel_action(parameters, graph_config, input, task_group_id, task_id, task):
    # Note that this is limited by the scopes afforded to generic actions to
    # only cancel tasks with the level-specific schedulerId.
    cancel_task(task_id, use_proxy=True)
