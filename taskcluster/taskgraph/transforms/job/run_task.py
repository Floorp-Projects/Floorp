# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running jobs that are invoked via the `run-task` script.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.job import run_job_using
from taskgraph.util.schema import Schema
from taskgraph.transforms.job.common import support_vcs_checkout
from voluptuous import Required, Any

run_task_schema = Schema({
    Required('using'): 'run-task',

    # if true, add a cache at ~worker/.cache, which is where things like pip
    # tend to hide their caches.  This cache is never added for level-1 jobs.
    Required('cache-dotcache', default=False): bool,

    # if true (the default), perform a checkout in /builds/worker/checkouts/gecko
    Required('checkout', default=True): bool,

    # The sparse checkout profile to use. Value is the filename relative to the
    # directory where sparse profiles are defined (build/sparse-profiles/).
    Required('sparse-profile', default=None): basestring,

    # if true, perform a checkout of a comm-central based branch inside the
    # gecko checkout
    Required('comm-checkout', default=False): bool,

    # The command arguments to pass to the `run-task` script, after the
    # checkout arguments.  If a list, it will be passed directly; otherwise
    # it will be included in a single argument to `bash -cx`.
    Required('command'): Any([basestring], basestring),
})


def common_setup(config, job, taskdesc):
    run = job['run']
    if run['checkout']:
        support_vcs_checkout(config, job, taskdesc,
                             sparse=bool(run['sparse-profile']))

    taskdesc['worker'].setdefault('env', {})['MOZ_SCM_LEVEL'] = config.params['level']


def add_checkout_to_command(run, command):
    if not run['checkout']:
        return

    command.append('--vcs-checkout=/builds/worker/checkouts/gecko')

    if run['sparse-profile']:
        command.append('--sparse-profile=build/sparse-profiles/%s' %
                       run['sparse-profile'])


@run_job_using("docker-worker", "run-task", schema=run_task_schema)
def docker_worker_run_task(config, job, taskdesc):
    run = job['run']
    worker = taskdesc['worker'] = job['worker']
    common_setup(config, job, taskdesc)

    if run.get('cache-dotcache'):
        worker['caches'].append({
            'type': 'persistent',
            'name': 'level-{level}-{project}-dotcache'.format(**config.params),
            'mount-point': '/builds/worker/.cache',
            'skip-untrusted': True,
        })

    run_command = run['command']
    if isinstance(run_command, basestring):
        run_command = ['bash', '-cx', run_command]
    command = ['/builds/worker/bin/run-task']
    add_checkout_to_command(run, command)
    if run['comm-checkout']:
        command.append('--comm-checkout=/builds/worker/checkouts/gecko/comm')
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
    add_checkout_to_command(run, command)
    command.append('--')
    command.extend(run_command)
    worker['command'] = command
