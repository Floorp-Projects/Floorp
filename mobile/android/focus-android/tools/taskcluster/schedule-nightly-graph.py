# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

import datetime
import jsone
import pipes
import yaml
import os
import slugid
import taskcluster

from git import Repo
from lib.tasks import schedule_task

ROOT = os.path.join(os.path.dirname(__file__), '../..')


def calculate_branch_and_head_rev(root):
    repo = Repo(root)
    branch = repo.head.reference
    return str(branch), str(branch.commit)


def make_decision_task(params):
    """Generate a basic decision task, based on the root .taskcluster.yml"""
    with open(os.path.join(ROOT, '.taskcluster.yml'), 'rb') as f:
        taskcluster_yml = yaml.safe_load(f)

    slugids = {}

    def as_slugid(name):
        if name not in slugids:
            slugids[name] = slugid.nice()
        return slugids[name]

    # provide a similar JSON-e context to what taskcluster-github provides
    context = {
        'tasks_for': 'cron',
        'cron': {
            'task_id': params['cron_task_id'],
        },
        'now': datetime.datetime.utcnow().isoformat()[:23] + 'Z',
        'as_slugid': as_slugid,
        'event': {
            'repository': {
                'clone_url': params['repository_url'],
            },
            'release': {
                'tag_name': params['head_rev'],
                'target_commitish': params['branch'],
            },
            'sender': {
                'login': 'TaskclusterHook'
            },
        },
    }

    rendered = jsone.render(taskcluster_yml, context)
    if len(rendered['tasks']) != 1:
        raise Exception("Expected .taskcluster.yml to only produce one cron task")
    task = rendered['tasks'][0]

    task_id = task.pop('taskId')
    return (task_id, task)


if __name__ == "__main__":
    queue = taskcluster.Queue({ 'baseUrl': 'http://taskcluster/queue/v1' })

    branch, head_rev = calculate_branch_and_head_rev(ROOT)

    params = {
        'repository_url': 'https://github.com/mozilla-mobile/focus-android',
        'head_rev': head_rev,
        'branch': branch,
        'cron_task_id': os.environ.get('CRON_TASK_ID', '<cron_task_id>')
    }
    decisionTaskId, decisionTask = make_decision_task(params)
    schedule_task(queue, decisionTaskId, decisionTask)
    print('All scheduled!')
