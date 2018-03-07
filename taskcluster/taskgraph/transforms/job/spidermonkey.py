# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running spidermonkey jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.schema import Schema
from voluptuous import Required, Any

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_artifacts,
    generic_worker_add_artifacts,
    docker_worker_add_gecko_vcs_env_vars,
    docker_worker_add_tooltool,
    support_vcs_checkout,
)

sm_run_schema = Schema({
    Required('using'): Any('spidermonkey', 'spidermonkey-package', 'spidermonkey-mozjs-crate',
                           'spidermonkey-rust-bindings'),

    # The SPIDERMONKEY_VARIANT
    Required('spidermonkey-variant'): basestring,
})


@run_job_using("docker-worker", "spidermonkey", schema=sm_run_schema)
@run_job_using("docker-worker", "spidermonkey-package", schema=sm_run_schema)
@run_job_using("docker-worker", "spidermonkey-mozjs-crate",
               schema=sm_run_schema)
@run_job_using("docker-worker", "spidermonkey-rust-bindings",
               schema=sm_run_schema)
def docker_worker_spidermonkey(config, job, taskdesc):
    run = job['run']

    worker = taskdesc['worker']
    worker['artifacts'] = []
    worker.setdefault('caches', []).append({
        'type': 'persistent',
        'name': 'level-{}-{}-build-spidermonkey-workspace'.format(
            config.params['level'], config.params['project']),
        'mount-point': "/builds/worker/workspace",
        'skip-untrusted': True,
    })

    docker_worker_add_artifacts(config, job, taskdesc)
    docker_worker_add_tooltool(config, job, taskdesc)

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_DISABLE': 'true',
        'SPIDERMONKEY_VARIANT': run['spidermonkey-variant'],
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
    })

    support_vcs_checkout(config, job, taskdesc)

    script = "build-sm.sh"
    if run['using'] == 'spidermonkey-package':
        script = "build-sm-package.sh"
    elif run['using'] == 'spidermonkey-mozjs-crate':
        script = "build-sm-mozjs-crate.sh"
    elif run['using'] == 'spidermonkey-rust-bindings':
        script = "build-sm-rust-bindings.sh"

    worker['command'] = [
        '/builds/worker/bin/run-task',
        '--vcs-checkout', '/builds/worker/workspace/build/src',
        '--',
        '/bin/bash',
        '-c',
        'cd /builds/worker && workspace/build/src/taskcluster/scripts/builder/%s' % script
    ]


@run_job_using("generic-worker", "spidermonkey", schema=sm_run_schema)
def generic_worker_spidermonkey(config, job, taskdesc):
    assert job['worker']['os'] == 'windows', 'only supports windows right now'

    run = job['run']

    worker = taskdesc['worker']

    generic_worker_add_artifacts(config, job, taskdesc)
    docker_worker_add_gecko_vcs_env_vars(config, job, taskdesc)

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_DISABLE': 'true',
        'SPIDERMONKEY_VARIANT': run['spidermonkey-variant'],
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'SCCACHE_DISABLE': "1",
        'WORK': ".",  # Override the defaults in build scripts
        'SRCDIR': "./src",  # with values suiteable for windows generic worker
        'UPLOAD_DIR': "./public/build"
    })

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

    hg_command = ['"c:\\Program Files\\Mercurial\\hg.exe"']
    hg_command.append('robustcheckout')
    hg_command.extend(['--sharebase', 'y:\\hg-shared'])
    hg_command.append('--purge')
    hg_command.extend(['--upstream', 'https://hg.mozilla.org/mozilla-unified'])
    hg_command.extend(['--revision', env['GECKO_HEAD_REV']])
    hg_command.append(env['GECKO_HEAD_REPOSITORY'])
    hg_command.append('.\\src')

    command = ['c:\\mozilla-build\\msys\\bin\\bash.exe '  # string concat
               '"./src/taskcluster/scripts/builder/%s"' % script]

    worker['command'] = []
    worker['command'].extend([
        ' '.join(hg_command),
        ' '.join(command)
    ])
