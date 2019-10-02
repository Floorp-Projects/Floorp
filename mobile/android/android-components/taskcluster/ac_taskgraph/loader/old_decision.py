# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import sys

current_dir = os.path.dirname(os.path.realpath(__file__))
project_dir = os.path.realpath(os.path.join(current_dir, '..', '..', '..'))
sys.path.append(project_dir)

from automation.taskcluster.decision_task import pr, push, release
from automation.taskcluster.lib.tasks import TaskBuilder
from automation.taskcluster.lib.build_config import components


def loader(kind, path, config, params, loaded_tasks):
    repo_url = params['head_repository']
    commit = params['head_rev']
    trust_level = int(params['level'])

    # XXX This logic will be replaced by what's in taskcluster/ci/config.yml,
    # once the migration is done
    build_worker_type = "mobile-{}-b-andrcmp".format(trust_level)
    beetmover_worker_type = "mobile-beetmover-v1" if trust_level == 3 else "mobile-beetmover-dev"

    builder = TaskBuilder(
        task_id=os.environ.get('TASK_ID'),
        repo_url=repo_url,
        branch=params['head_ref'],
        commit=commit,
        owner=params['owner'],
        source='{}/raw/{}/.taskcluster.yml'.format(repo_url, commit),
        scheduler_id='mobile-level-{}'.format(trust_level),
        tasks_priority='highest',    # TODO parametrize
        build_worker_type=build_worker_type,
        beetmover_worker_type=beetmover_worker_type,
    )

    tasks_for = params['tasks_for']

    artifacts_info = components()
    if tasks_for == 'github-release':
        artifacts_info = [info for info in artifacts_info if info['shouldPublish']]
    if len(artifacts_info) == 0:
        raise ValueError("Could not get module names from gradle")

    is_staging = trust_level != 3

    if tasks_for == 'github-pull-request':
        ordered_groups_of_tasks = pr(builder, artifacts_info)
    elif tasks_for == 'github-push':
        ordered_groups_of_tasks = push(builder, artifacts_info)
    elif tasks_for == 'github-release':
        ordered_groups_of_tasks = release(builder, artifacts_info, is_snapshot=False, is_staging=is_staging)
    elif tasks_for == 'cron':
        if params['target_tasks_method'] == 'snapshot':
            ordered_groups_of_tasks = release(builder, artifacts_info, is_snapshot=True, is_staging=is_staging)
        else:
            raise NotImplementedError('Unsupported target_tasks_method "{}"'.format(params['target_tasks_method']))
    else:
        raise NotImplementedError('Unsupported tasks_for "{}"'.format(tasks_for))

    for task in ordered_groups_of_tasks:
        yield task
