# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running spidermonkey jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.schema import Schema
from voluptuous import Required, Any, Optional

from taskgraph.transforms.job import (
    run_job_using,
    configure_taskdesc_for_run,
)
from taskgraph.transforms.job.common import (
    docker_worker_add_artifacts,
    generic_worker_add_artifacts,
    docker_worker_add_tooltool,
)

sm_run_schema = Schema({
    Required('using'): Any('spidermonkey', 'spidermonkey-package', 'spidermonkey-mozjs-crate',
                           'spidermonkey-rust-bindings'),

    # SPIDERMONKEY_VARIANT and SPIDERMONKEY_PLATFORM
    Required('spidermonkey-variant'): basestring,
    Optional('spidermonkey-platform'): basestring,

    # Base work directory used to set up the task.
    Required('workdir'): basestring,
})


@run_job_using("docker-worker", "spidermonkey", schema=sm_run_schema)
@run_job_using("docker-worker", "spidermonkey-package", schema=sm_run_schema)
@run_job_using("docker-worker", "spidermonkey-mozjs-crate",
               schema=sm_run_schema)
@run_job_using("docker-worker", "spidermonkey-rust-bindings",
               schema=sm_run_schema)
def docker_worker_spidermonkey(config, job, taskdesc):
    run = job['run']

    worker = taskdesc['worker'] = job['worker']
    worker['artifacts'] = []
    worker.setdefault('caches', []).append({
        'type': 'persistent',
        'name': '{}-build-spidermonkey-workspace'.format(config.params['project']),
        'mount-point': "{workdir}/workspace".format(**run),
        'skip-untrusted': True,
    })

    docker_worker_add_artifacts(config, job, taskdesc)
    docker_worker_add_tooltool(config, job, taskdesc)

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_DISABLE': 'true',
        'SPIDERMONKEY_VARIANT': run.pop('spidermonkey-variant'),
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'GECKO_PATH': '{}/workspace/build/src'.format(run['workdir'])
    })
    if 'spidermonkey-platform' in run:
        env['SPIDERMONKEY_PLATFORM'] = run.pop('spidermonkey-platform')

    script = "build-sm.sh"
    if run['using'] == 'spidermonkey-package':
        script = "build-sm-package.sh"
    elif run['using'] == 'spidermonkey-mozjs-crate':
        script = "build-sm-mozjs-crate.sh"
    elif run['using'] == 'spidermonkey-rust-bindings':
        script = "build-sm-rust-bindings.sh"

    run['using'] = 'run-task'
    run['cwd'] = run['workdir']
    run['command'] = [
        'workspace/build/src/taskcluster/scripts/builder/{script}'.format(
            script=script)
    ]

    configure_taskdesc_for_run(config, job, taskdesc, worker['implementation'])


@run_job_using("generic-worker", "spidermonkey", schema=sm_run_schema)
def generic_worker_spidermonkey(config, job, taskdesc):
    assert job['worker']['os'] == 'windows', 'only supports windows right now'

    run = job['run']

    worker = taskdesc['worker'] = job['worker']

    generic_worker_add_artifacts(config, job, taskdesc)

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_DISABLE': 'true',
        'SPIDERMONKEY_VARIANT': run.pop('spidermonkey-variant'),
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'SCCACHE_DISABLE': "1",
        'WORK': ".",  # Override the defaults in build scripts
        'GECKO_PATH': "./src",  # with values suiteable for windows generic worker
        'UPLOAD_DIR': "./public/build"
    })
    if 'spidermonkey-platform' in run:
        env['SPIDERMONKEY_PLATFORM'] = run.pop('spidermonkey-platform')

    script = "build-sm.sh"
    if run['using'] == 'spidermonkey-package':
        script = "build-sm-package.sh"
        # Don't allow untested configurations yet
        raise Exception("spidermonkey-package is not a supported configuration")
    elif run['using'] == 'spidermonkey-mozjs-crate':
        script = "build-sm-mozjs-crate.sh"
        # Don't allow untested configurations yet
        raise Exception("spidermonkey-mozjs-crate is not a supported configuration")
    elif run['using'] == 'spidermonkey-rust-bindings':
        script = "build-sm-rust-bindings.sh"
        # Don't allow untested configurations yet
        raise Exception("spidermonkey-rust-bindings is not a supported configuration")

    run['using'] = 'run-task'
    run['command'] = ['c:\\mozilla-build\\msys\\bin\\bash.exe '  # string concat
                      '"./src/taskcluster/scripts/builder/%s"' % script]

    configure_taskdesc_for_run(config, job, taskdesc, worker['implementation'])
