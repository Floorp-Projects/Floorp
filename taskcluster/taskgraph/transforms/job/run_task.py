# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running jobs that are invoked via the `run-task` script.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.job import run_job_using
from taskgraph.util.schema import Schema
from taskgraph.transforms.job.common import support_use_artifacts, support_vcs_checkout
from voluptuous import Required, Any

run_task_schema = Schema({
    Required('using'): 'run-task',

    # if true, add a cache at ~worker/.cache, which is where things like pip
    # tend to hide their caches.  This cache is never added for level-1 jobs.
    Required('cache-dotcache'): bool,

    # if true (the default), perform a checkout in {workdir}/checkouts/gecko
    Required('checkout'): bool,

    # The sparse checkout profile to use. Value is the filename relative to the
    # directory where sparse profiles are defined (build/sparse-profiles/).
    Required('sparse-profile'): Any(basestring, None),

    # if true, perform a checkout of a comm-central based branch inside the
    # gecko checkout
    Required('comm-checkout'): bool,

    # maps a dependency to a list of artifact names to use from that dependency.
    # E.g: {"build": ["target.tar.bz2"]}
    # In the above example, the artifact would be downloaded to:
    # $USE_ARTIFACT_PATH/build/target.tar.bz2
    Required('use-artifacts'): Any(None, {
        basestring: [basestring],
    }),

    # The command arguments to pass to the `run-task` script, after the
    # checkout arguments.  If a list, it will be passed directly; otherwise
    # it will be included in a single argument to `bash -cx`.
    Required('command'): Any([basestring], basestring),

    # Base work directory used to set up the task.
    Required('workdir'): basestring,
})


def common_setup(config, job, taskdesc):
    run = job['run']
    if run['checkout']:
        support_vcs_checkout(config, job, taskdesc,
                             sparse=bool(run['sparse-profile']))

    if run['use-artifacts']:
        support_use_artifacts(config, job, taskdesc, run['use-artifacts'])

    taskdesc['worker'].setdefault('env', {})['MOZ_SCM_LEVEL'] = config.params['level']


def add_checkout_to_command(run, command):
    if not run['checkout']:
        return

    command.append('--vcs-checkout={workdir}/checkouts/gecko'.format(**run))

    if run['sparse-profile']:
        command.append('--sparse-profile=build/sparse-profiles/%s' %
                       run['sparse-profile'])


defaults = {
    'cache-dotcache': False,
    'checkout': True,
    'comm-checkout': False,
    'sparse-profile': None,
    'use-artifacts': None,
}


@run_job_using("docker-worker", "run-task", schema=run_task_schema, defaults=defaults)
def docker_worker_run_task(config, job, taskdesc):
    run = job['run']
    worker = taskdesc['worker'] = job['worker']
    common_setup(config, job, taskdesc)

    if run.get('cache-dotcache'):
        worker['caches'].append({
            'type': 'persistent',
            'name': 'level-{level}-{project}-dotcache'.format(**config.params),
            'mount-point': '{workdir}/.cache'.format(**run),
            'skip-untrusted': True,
        })

    run_command = run['command']
    if isinstance(run_command, basestring):
        run_command = ['bash', '-cx', run_command]
    command = ['{workdir}/bin/run-task'.format(**run)]
    add_checkout_to_command(run, command)
    if run['comm-checkout']:
        command.append('--comm-checkout={workdir}/checkouts/gecko/comm'.format(**run))
    command.append('--fetch-hgfingerprint')
    command.append('--')
    command.extend(run_command)
    worker['command'] = command


@run_job_using("native-engine", "run-task", schema=run_task_schema, defaults=defaults)
def native_engine_run_task(config, job, taskdesc):
    run = job['run']
    worker = taskdesc['worker'] = job['worker']
    common_setup(config, job, taskdesc)

    worker['context'] = '{}/raw-file/{}/taskcluster/scripts/run-task'.format(
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
