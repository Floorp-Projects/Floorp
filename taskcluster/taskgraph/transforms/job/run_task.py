# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running jobs that are invoked via the `run-task` script.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.task import taskref_or_string
from taskgraph.transforms.job import run_job_using
from taskgraph.util.schema import Schema
from taskgraph.transforms.job.common import (
        docker_worker_add_tooltool,
        support_vcs_checkout
)
from voluptuous import Any, Optional, Required

run_task_schema = Schema({
    Required('using'): 'run-task',

    # if true, add a cache at ~worker/.cache, which is where things like pip
    # tend to hide their caches.  This cache is never added for level-1 jobs.
    # TODO Once bug 1526028 is fixed, this and 'use-caches' should be merged.
    Required('cache-dotcache'): bool,

    # Whether or not to use caches.
    Optional('use-caches'): bool,

    # if true (the default), perform a checkout of gecko on the worker
    Required('checkout'): bool,

    # The sparse checkout profile to use. Value is the filename relative to the
    # directory where sparse profiles are defined (build/sparse-profiles/).
    Required('sparse-profile'): Any(basestring, None),

    # if true, perform a checkout of a comm-central based branch inside the
    # gecko checkout
    Required('comm-checkout'): bool,

    # The command arguments to pass to the `run-task` script, after the
    # checkout arguments.  If a list, it will be passed directly; otherwise
    # it will be included in a single argument to `bash -cx`.
    Required('command'): Any([taskref_or_string], taskref_or_string),

    # Base work directory used to set up the task.
    Required('workdir'): basestring,

    # If not false, tooltool downloads will be enabled via relengAPIProxy
    # for either just public files, or all files. Only supported on
    # docker-worker.
    Required('tooltool-downloads'): Any(
        False,
        'public',
        'internal',
    ),

})


def common_setup(config, job, taskdesc, command):
    run = job['run']
    if run['checkout']:
        support_vcs_checkout(config, job, taskdesc,
                             sparse=bool(run['sparse-profile']))
        command.append('--gecko-checkout={}'.format(taskdesc['worker']['env']['GECKO_PATH']))

    if run['sparse-profile']:
        command.append('--gecko-sparse-profile=build/sparse-profiles/%s' %
                       run['sparse-profile'])

    taskdesc['worker'].setdefault('env', {})['MOZ_SCM_LEVEL'] = config.params['level']


worker_defaults = {
    'cache-dotcache': False,
    'checkout': True,
    'comm-checkout': False,
    'sparse-profile': None,
    'tooltool-downloads': False,
}


def script_url(config, script):
    return '{}/raw-file/{}/taskcluster/scripts/{}'.format(
                config.params['head_repository'],
                config.params['head_rev'],
                script)


@run_job_using("docker-worker", "run-task", schema=run_task_schema, defaults=worker_defaults)
def docker_worker_run_task(config, job, taskdesc):
    run = job['run']
    worker = taskdesc['worker'] = job['worker']
    command = ['/builds/worker/bin/run-task']
    common_setup(config, job, taskdesc, command)

    if run['tooltool-downloads']:
        internal = run['tooltool-downloads'] == 'internal'
        docker_worker_add_tooltool(config, job, taskdesc, internal=internal)

    if run.get('cache-dotcache'):
        worker['caches'].append({
            'type': 'persistent',
            'name': '{project}-dotcache'.format(**config.params),
            'mount-point': '{workdir}/.cache'.format(**run),
            'skip-untrusted': True,
        })

    run_command = run['command']
    # dict is for the case of `{'task-reference': basestring}`.
    if isinstance(run_command, (basestring, dict)):
        run_command = ['bash', '-cx', run_command]
    if run['comm-checkout']:
        command.append('--comm-checkout={workdir}/checkouts/gecko/comm'.format(**run))
    command.append('--fetch-hgfingerprint')
    command.append('--')
    command.extend(run_command)
    worker['command'] = command


@run_job_using("generic-worker", "run-task", schema=run_task_schema, defaults=worker_defaults)
def generic_worker_run_task(config, job, taskdesc):
    run = job['run']
    worker = taskdesc['worker'] = job['worker']
    is_win = worker['os'] == 'windows'
    is_mac = worker['os'] == 'macosx'

    if is_win:
        command = ['C:/mozilla-build/python3/python3.exe', 'run-task']
    elif is_mac:
        command = ['/tools/python37/bin/python3.7', 'run-task']
        if job['worker-type'].endswith('1014'):
            command = ['/usr/local/bin/python3', 'run-task']
    else:
        command = ['./run-task']

    common_setup(config, job, taskdesc, command)

    worker.setdefault('mounts', [])
    if run.get('cache-dotcache'):
        worker['mounts'].append({
            'cache-name': '{project}-dotcache'.format(**config.params),
            'directory': '{workdir}/.cache'.format(**run),
        })
    worker['mounts'].append({
        'content': {
            'url': script_url(config, 'run-task'),
        },
        'file': './run-task',
    })
    if worker.get('env', {}).get('MOZ_FETCHES'):
        worker['mounts'].append({
            'content': {
                'url': script_url(config, 'misc/fetch-content'),
            },
            'file': './fetch-content',
        })

    run_command = run['command']
    if isinstance(run_command, basestring):
        if is_win:
            run_command = '"{}"'.format(run_command)
        run_command = ['bash', '-cx', run_command]

    command.append('--')
    command.extend(run_command)

    if is_win:
        worker['command'] = [' '.join(command)]
    else:
        worker['command'] = [
            ['chmod', '+x', 'run-task'],
            command,
        ]
