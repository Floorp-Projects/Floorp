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
from taskgraph.util.taskcluster import send_email
from .registry import register_callback_action

logger = logging.getLogger(__name__)

EMAIL_SUBJECT = 'Your Interactive Task for {label}'
EMAIL_CONTENT = '''\
As you requested, Firefox CI has created an interactive task to run {label}
on revision {revision} in {repo}. Click the button below to connect to the
task. You may need to wait for it to begin running.
'''

@register_callback_action(
    title='Create Interactive Task',
    name='create-interactive',
    symbol='create-inter',
    kind='hook',
    generic=True,
    description=(
        'Create a a copy of the task that you can interact with'
    ),
    order=1,
    context=[{'worker-implementation': 'docker-worker'}],
    schema={
        'type': 'object',
        'properties': {
            'notify': {
                'type': 'string',
                'format': 'email',
                'title': 'Who to notify of the pending interactive task',
                'description': 'Enter your email here to get an email containing a link to interact with the task',
                # include a default for ease of users' editing
                'default': 'noreply@noreply.mozilla.org',
            },
        },
        'additionalProperties': False,
    },
)
def create_interactive_action(parameters, graph_config, input, task_group_id, task_id, task):
    # fetch the original task definition from the taskgraph, to avoid
    # creating interactive copies of unexpected tasks.  Note that this only applies
    # to docker-worker tasks, so we can assume the docker-worker payload format.
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config)

    label = task['metadata']['name']
    def edit(task):
        if task.label != label:
            return task
        task_def = task.task

        # drop task routes (don't index this!)
        task_def['routes'] = []

        # only try this once
        task_def['retries'] = 0

        # short expirations, at least 3 hour maxRunTime
        task_def['deadline'] = {'relative-datestamp': '12 hours'}
        task_def['created'] = {'relative-datestamp': '0 hours'}
        task_def['expires'] = {'relative-datestamp': '1 day'}
        task_def['payload']['maxRunTime'] = max(3600 * 3, task_def['payload'].get('maxRunTime', 0))

        # no caches
        task_def['scopes'] = [s for s in task_def['scopes'] if not s.startswith('docker-worker:cache:')]
        task_def['payload']['cache'] = {}

        # no artifacts
        task_def['payload']['artifacts'] = {}

        # enable interactive mode
        task_def['payload'].setdefault('features', {})['interactive'] = True
        task_def['payload'].setdefault('env', {})['TASKCLUSTER_INTERACTIVE'] = 'true'

        return task

    # Create the task and any of its dependencies. This uses a new taskGroupId to avoid
    # polluting the existing taskGroup with interactive tasks.
    label_to_taskid = create_tasks([label], full_task_graph, label_to_taskid, parameters, modifier=edit)

    taskId = label_to_taskid[label]

    if input and 'notify' in input:
        email = input['notify']
        # no point sending to a noreply address!
        if email == 'noreply@noreply.mozilla.org':
            return

        info = {
            'url': 'https://tools.taskcluster.net/tasks/{}/connect'.format(taskId),
            'label': label,
            'revision': parameters['head_rev'],
            'repo': parameters['head_repository'],
        }
        send_email(
            email,
            subject=EMAIL_SUBJECT.format(**info),
            content=EMAIL_CONTENT.format(**info),
            link={
                'text': 'Connect',
                'href': info['url'],
            },
            use_proxy=True)

