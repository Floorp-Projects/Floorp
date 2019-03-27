# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import jsone
import os
import slugid
import taskcluster
import yaml

from git import Repo
from lib.tasks import schedule_task

ROOT = os.path.join(os.path.dirname(__file__), '../..')


class InvalidGithubRepositoryError(Exception):
    pass


def calculate_git_references(root):
    repo = Repo(root)
    remote = repo.remote()
    branch = repo.head.reference

    if not remote.url.startswith('https://github.com'):
        raise InvalidGithubRepositoryError('expected remote to be a GitHub repository (accessed via HTTPs)')

    html_url = remote.url[:-4] if remote.url.endswith('.git') else remote.url
    return html_url, str(branch), str(branch.commit)


def make_decision_task(params):
    """Generate a basic decision task, based on the root .taskcluster.yml"""
    with open(os.path.join(ROOT, '.taskcluster.yml'), 'rb') as f:
        taskcluster_yml = yaml.safe_load(f)

    slugids = {}

    def as_slugid(name):
        if name not in slugids:
            slugids[name] = slugid.nice()
        return slugids[name]

    repository_parts = params['html_url'].split('/')
    repository_full_name = '/'.join((repository_parts[-2], repository_parts[-1]))

    # provide a similar JSON-e context to what taskcluster-github provides
    context = {
        'tasks_for': 'cron',
        'cron': {
            'task_id': params['cron_task_id']
        },
        'now': datetime.datetime.utcnow().isoformat()[:23] + 'Z',
        'as_slugid': as_slugid,
        'event': {
            'repository': {
                'html_url': params['html_url'],
                'full_name': repository_full_name,
            },
            'release': {
                'tag_name': params['head_rev'],
                'target_commitish': params['branch'],
            },
            'sender': {
                'login': 'TaskclusterHook',
            }
        }
    }

    rendered = jsone.render(taskcluster_yml, context)
    if len(rendered['tasks']) != 1:
        raise Exception('Expected .taskcluster.yml to only produce one cron task')
    task = rendered['tasks'][0]

    task_id = task.pop('taskId')
    return task_id, task


def schedule():
    queue = taskcluster.Queue({'baseUrl': 'http://taskcluster/queue/v1'})

    html_url, branch, head_rev = calculate_git_references(ROOT)
    params = {
        'html_url': html_url,
        'head_rev': head_rev,
        'branch': branch,
        'cron_task_id': os.environ.get('CRON_TASK_ID', '<cron_task_id>')
    }
    decision_task_id, decision_task = make_decision_task(params)
    schedule_task(queue, decision_task_id, decision_task)
    print('All scheduled!')


if __name__ == '__main__':
    schedule()
