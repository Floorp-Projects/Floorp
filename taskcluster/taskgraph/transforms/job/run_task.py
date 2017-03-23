# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running jobs that are invoked via the `run-task` script.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.job import run_job_using
from taskgraph.util.schema import Schema
from taskgraph.transforms.job.common import (
    add_build_dependency,
    support_vcs_checkout,
)
from voluptuous import Required, Any

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

    # Whether the job requires a build artifact or not. If True, the task
    # will depend on a build task and run-task will download and set up the
    # installer. Build labels are determined by the `dependent-build-platforms`
    # config in kind.yml.
    Required('requires-build', default=False): bool,
})


def common_setup(config, job, taskdesc):
    run = job['run']
    if run['checkout']:
        support_vcs_checkout(config, job, taskdesc)

    if run['requires-build']:
        add_build_dependency(config, job, taskdesc)


@run_job_using("docker-worker", "run-task", schema=run_task_schema)
def docker_worker_run_task(config, job, taskdesc):
    run = job['run']
    worker = taskdesc['worker'] = job['worker']
    common_setup(config, job, taskdesc)

    if run.get('cache-dotcache') and int(config.params['level']) > 1:
        worker['caches'].append({
            'type': 'persistent',
            'name': 'level-{level}-{project}-dotcache'.format(**config.params),
            'mount-point': '/home/worker/.cache',
        })

    run_command = run['command']
    if isinstance(run_command, basestring):
        run_command = ['bash', '-cx', run_command]
    command = ['/home/worker/bin/run-task']
    if run['checkout']:
        command.append('--vcs-checkout=~/checkouts/gecko')
    command.append('--fetch-hgfingerprint')
    command.append('--')
    command.extend(run_command)
    worker['command'] = command


@run_job_using("native-engine", "run-task", schema=run_task_schema)
def native_engine_run_task(config, job, taskdesc):
    run = job['run']
    worker = taskdesc['worker'] = job['worker']
    common_setup(config, job, taskdesc)

    worker['context'] = '{}/raw-file/{}/taskcluster/docker/recipes/run-task'.format(
        config.params['head_repository'], config.params['head_rev']
    )

    if run.get('cache-dotcache'):
        raise Exception("No cache support on native-worker; can't use cache-dotcache")

    run_command = run['command']
    if isinstance(run_command, basestring):
        run_command = ['bash', '-cx', run_command]
    command = ['./run-task']
    if run['checkout']:
        command.append('--vcs-checkout=~/checkouts/gecko')
    command.append('--')
    command.extend(run_command)
    worker['command'] = command
