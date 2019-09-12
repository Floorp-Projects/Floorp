# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import sys

current_dir = os.path.dirname(os.path.realpath(__file__))
project_dir = os.path.realpath(os.path.join(current_dir, '..', '..', '..'))
sys.path.append(project_dir)

from automation.taskcluster.decision_task import pr
from automation.taskcluster.lib.tasks import TaskBuilder


def loader(kind, path, config, params, loaded_tasks):
    repo_url = params['head_repository']
    commit = params['head_rev']
    trust_level = int(params['level'])

    builder = TaskBuilder(
        task_id=os.environ.get('TASK_ID'),
        repo_url=repo_url,
        git_ref=params['head_ref'],
        short_head_branch=params['head_ref'],
        commit=commit,
        owner=params['owner'],
        source='{}/raw/{}/.taskcluster.yml'.format(repo_url, commit),
        scheduler_id='mobile-level-{}'.format(trust_level),
        tasks_priority='highest',    # TODO parametrize
        date_string=params['moz_build_date'],
        trust_level=trust_level,
    )

    tasks_for = params['tasks_for']
    if tasks_for == 'github-pull-request':
        ordered_groups_of_tasks = pr(builder)
    else:
        raise NotImplementedError('Unsupported tasks_for "{}"'.format(tasks_for))

    for task in ordered_groups_of_tasks:
        yield task
