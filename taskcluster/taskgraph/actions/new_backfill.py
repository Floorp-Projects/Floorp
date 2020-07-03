# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging


from taskgraph.util.taskcluster import get_task_definition
from .registry import register_callback_action
from .util import (
    create_tasks,
    fetch_graph_and_labels,
    get_decision_task_id,
    get_pushes_from_params_input,
    trigger_action,
)

logger = logging.getLogger(__name__)


def input_for_support_action(task):
    '''Generate input for action to be scheduled.

    Define what label to schedule with 'label'.
    '''
    return {'label': task['metadata']['name']}


@register_callback_action(
    title='Backfill (new)',
    name='new-backfill',
    permission='backfill',
    symbol='Bk',
    description=('Trigger the selected task on previous pushes for the same project.'),
    order=200,
    # When we are ready to promote this new backfill we can support all tasks
    context=[{"kind": "test"}],
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
            }
        },
        'additionalProperties': False
    },
    available=lambda parameters: True
)
def new_backfill_action(parameters, graph_config, input, task_group_id, task_id):
    '''
    This action takes a task ID and schedules it on previous pushes (via support action).

    To execute this action locally follow the documentation here:
    https://firefox-source-docs.mozilla.org/taskcluster/actions.html#testing-the-action-locally
    '''
    task = get_task_definition(task_id)
    pushes = get_pushes_from_params_input(parameters, input)

    for push_id in pushes:
        try:
            trigger_action(
                action_name='backfill-task',
                # This lets the action know on which push we want to add a new task
                decision_task_id=get_decision_task_id(parameters['project'], push_id),
                input=input_for_support_action(task),
            )
        except Exception as e:
            logger.warning('Failure to trigger action for {}'.format(push_id))
            logger.exception(e)


@register_callback_action(
    name='backfill-task',
    title='Backfill task on a push.',
    permission='backfill',
    symbol='backfill-task',
    description='This action is normally scheduled by the backfill action. '
                'The intent is to schedule a task on previous pushes.',
    order=500,
    context=[],
    schema={
        'type': 'object',
        'properties': {
            'label': {
                'type': 'string',
                'description': 'A task label'
            }
        }
    }
)
def add_test_task(parameters, graph_config, input, task_group_id, task_id):
    '''
    This action is normally scheduled by the backfill action. The intent is to schedule a test
    task on previous pushes.

    The push in which we want to schedule a new task is defined by the parameters object.

    To execute this action locally follow the documentation here:
    https://firefox-source-docs.mozilla.org/taskcluster/actions.html#testing-the-action-locally
    '''
    # This step takes a lot of time when executed locally
    logger.info("Retreving the full task graph and labels.")
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config)

    logger.info("Creating tasks...")
    create_tasks(
        graph_config,
        [input.get('label')],
        full_task_graph,
        label_to_taskid,
        parameters,
        decision_task_id=decision_task_id
    )
