# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running jobs that are invoked via the `run-task` script.
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy

from taskgraph.transforms.job import run_job_using
from voluptuous import Schema, Required, Any

run_task_schema = Schema({
    Required('using'): 'run-task',

    # if true, add a cache at ~worker/.cache, which is where things like pip
    # tend to hide their caches.  This cache is never added for level-1 jobs.
    Required('cache-dotcache', default=False): bool,

    # if true (the default), perform a checkout in /home/worker/checkouts/gecko
    Required('checkout', default=True): bool,

    # The command arguments to pass to the `run-task` script, after the
    # checkout arguments.  If a list, it will be passed directly; otherwise
    # it will be included in a single argument to `bash -cx`.
    Required('command'): Any([basestring], basestring),
})


@run_job_using("docker-worker", "run-task", schema=run_task_schema)
def docker_worker_run_task(config, job, taskdesc):
    run = job['run']
    checkout = run['checkout']

    worker = taskdesc['worker'] = copy.deepcopy(job['worker'])

    if checkout:
        worker['caches'] = [{
            'type': 'persistent',
            'name': 'level-{}-hg-shared'.format(config.params['level']),
            'mount-point': "/home/worker/hg-shared",
        }, {
            'type': 'persistent',
            'name': 'level-{}-checkouts'.format(config.params['level']),
            'mount-point': "/home/worker/checkouts",
        }]

    if run.get('cache-dotcache') and int(config.params['level']) > 1:
        worker['caches'].append({
            'type': 'persistent',
            'name': 'level-{level}-{project}-dotcache'.format(**config.params),
            'mount-point': '/home/worker/.cache',
        })

    env = worker['env'] = {}
    env.update({
        'GECKO_BASE_REPOSITORY': config.params['base_repository'],
        'GECKO_HEAD_REPOSITORY': config.params['head_repository'],
        'GECKO_HEAD_REV': config.params['head_rev'],
    })

    # give the task access to the hgfingerprint secret
    if checkout:
        taskdesc['scopes'].append('secrets:get:project/taskcluster/gecko/hgfingerprint')
        worker['taskcluster-proxy'] = True

    run_command = run['command']
    if isinstance(run_command, basestring):
        run_command = ['bash', '-cx', run_command]
    command = ['/home/worker/bin/run-task']
    if checkout:
        command.append('--vcs-checkout=/home/worker/checkouts/gecko')
    command.append('--')
    command.extend(run_command)
    worker['command'] = command
