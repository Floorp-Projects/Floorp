# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

import jsone
import pipes
import os
import slugid

from taskgraph.util.time import current_json_time
from taskgraph.util.hg import find_hg_revision_push_info
from taskgraph.util.yaml import load_yaml


def run_decision_task(job, params, root):
    arguments = []
    if 'target-tasks-method' in job:
        arguments.append('--target-tasks-method={}'.format(job['target-tasks-method']))
    if job.get('optimize-target-tasks') is not None:
        arguments.append('--optimize-target-tasks={}'.format(
            str(job['optimize-target-tasks']).lower(),
        ))
    if 'include-push-tasks' in job:
        arguments.append('--include-push-tasks')
    if 'rebuild-kinds' in job:
        for kind in job['rebuild-kinds']:
            arguments.append('--rebuild-kind={}'.format(kind))
    if job.get('android-release-type') is not None:
        arguments.append('--android-release-type={}'.format(
            str(job['android-release-type']).lower(),
        ))
    return [
        make_decision_task(
            params,
            symbol=job['treeherder-symbol'],
            arguments=arguments,
            root=root),
    ]


def make_decision_task(params, root, symbol, arguments=[]):
    """Generate a basic decision task, based on the root .taskcluster.yml"""
    taskcluster_yml = load_yaml(root, '.taskcluster.yml')

    push_info = find_hg_revision_push_info(
        params['repository_url'],
        params['head_rev'])

    # provide a similar JSON-e context to what mozilla-taskcluster provides:
    # https://docs.taskcluster.net/reference/integrations/mozilla-taskcluster/docs/taskcluster-yml
    # but with a different tasks_for and an extra `cron` section
    context = {
        'tasks_for': 'cron',
        'repository': {
            'url': params['repository_url'],
            'project': params['project'],
            'level': params['level'],
        },
        'push': {
            'revision': params['head_rev'],
            # remainder are fake values, but the decision task expects them anyway
            'pushlog_id': push_info['pushid'],
            'pushdate': push_info['pushdate'],
            'owner': 'cron',
        },
        'cron': {
            'task_id': os.environ.get('TASK_ID', '<cron task id>'),
            'job_name': params['job_name'],
            'job_symbol': symbol,
            # args are shell-quoted since they are given to `bash -c`
            'quoted_args': ' '.join(pipes.quote(a) for a in arguments),
        },
        'now': current_json_time(),
        'ownTaskId': slugid.nice(),
    }

    rendered = jsone.render(taskcluster_yml, context)
    if len(rendered['tasks']) != 1:
        raise Exception("Expected .taskcluster.yml to only produce one cron task")
    task = rendered['tasks'][0]

    task_id = task.pop('taskId')
    return (task_id, task)
