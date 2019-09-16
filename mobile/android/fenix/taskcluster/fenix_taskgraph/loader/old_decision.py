# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import datetime
import os
import re
import sys

current_dir = os.path.dirname(os.path.realpath(__file__))
project_dir = os.path.realpath(os.path.join(current_dir, '..', '..', '..'))
sys.path.append(project_dir)

from automation.taskcluster.decision_task import (
    pr,
    push,
    raptor,
    nightly_to_production_app,
    release,
    release_as_fennec,
)
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

    is_staging = trust_level != 3

    tasks_for = params['tasks_for']
    if tasks_for == 'github-pull-request':
        ordered_groups_of_tasks = pr(builder)
    elif tasks_for == 'github-push':
        ordered_groups_of_tasks = push(builder)
    elif tasks_for == 'github-release':
        git_tag = os.environ['GIT_TAG']
        version = git_tag[1:]  # remove prefixed "v"
        beta_semver = re.compile(r'^v\d+\.\d+\.\d+-beta\.\d+$')
        production_semver = re.compile(r'^v\d+\.\d+\.\d+(-rc\.\d+)?$')
        if beta_semver.match(git_tag):
            ordered_groups_of_tasks = release(builder, 'beta', 'geckoBeta', is_staging, version)
        elif production_semver.match(git_tag):
            ordered_groups_of_tasks = release(builder, 'production', 'geckoBeta', is_staging, version)
        else:
            raise ValueError('Github tag must be in semver format and prefixed with a "v", '
                             'e.g.: "v1.0.0-beta.0" (beta), "v1.0.0-rc.0" (production) or "v1.0.0" (production)')
    elif tasks_for == 'cron':
        target_tasks_method = params['target_tasks_method']
        if target_tasks_method == 'raptor':
            ordered_groups_of_tasks = raptor(builder, is_staging)
        elif target_tasks_method == 'nightly':
            now = datetime.datetime.now().strftime('%y%m%d %H:%M')
            nightly_version = 'Nightly {}'.format(now)
            ordered_groups_of_tasks = release(builder, 'nightly', 'geckoNightly', is_staging, nightly_version) \
            + nightly_to_production_app(builder, is_staging, nightly_version)
            ordered_groups_of_tasks += release_as_fennec(builder, is_staging, 'Signed-as-Fennec Nightly {}'.format(now))
        else:
            raise NotImplementedError('Unsupported task_name "{}"'.format(params))
    else:
        raise NotImplementedError('Unsupported tasks_for "{}"'.format(tasks_for))

    for task in ordered_groups_of_tasks:
        yield task
