# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running toolchain-building jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.schema import Schema
from voluptuous import Optional, Required, Any

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_tc_vcs_cache,
    docker_worker_add_gecko_vcs_env_vars,
    support_vcs_checkout,
)
from taskgraph.util.hash import hash_paths
from taskgraph import GECKO


TOOLCHAIN_INDEX = 'gecko.cache.level-{level}.toolchains.v1.{name}.{digest}'

toolchain_run_schema = Schema({
    Required('using'): 'toolchain-script',

    # the script (in taskcluster/scripts/misc) to run
    Required('script'): basestring,

    # If not false, tooltool downloads will be enabled via relengAPIProxy
    # for either just public files, or all files.  Not supported on Windows
    Required('tooltool-downloads', default=False): Any(
        False,
        'public',
        'internal',
    ),

    # Paths/patterns pointing to files that influence the outcome of a
    # toolchain build.
    Optional('resources'): [basestring],
})


def add_optimizations(config, run, taskdesc):
    files = list(run.get('resources', []))
    # This file
    files.append('taskcluster/taskgraph/transforms/job/toolchain.py')
    # The script
    files.append('taskcluster/scripts/misc/{}'.format(run['script']))

    label = taskdesc['label']
    subs = {
        'name': label.replace('toolchain-', '').split('/')[0],
        'digest': hash_paths(GECKO, files),
    }

    optimizations = taskdesc.setdefault('optimizations', [])

    # We'll try to find a cached version of the toolchain at levels above
    # and including the current level, starting at the highest level.
    for level in reversed(range(int(config.params['level']), 4)):
        subs['level'] = level
        optimizations.append(['index-search', TOOLCHAIN_INDEX.format(**subs)])

    # ... and cache at the lowest level.
    taskdesc.setdefault('routes', []).append(
        'index.{}'.format(TOOLCHAIN_INDEX.format(**subs)))


@run_job_using("docker-worker", "toolchain-script", schema=toolchain_run_schema)
def docker_worker_toolchain(config, job, taskdesc):
    run = job['run']
    taskdesc['run-on-projects'] = ['trunk', 'try']

    worker = taskdesc['worker']
    worker['artifacts'] = []
    worker['caches'] = []

    worker['artifacts'].append({
        'name': 'public',
        'path': '/home/worker/workspace/artifacts/',
        'type': 'directory',
    })

    docker_worker_add_tc_vcs_cache(config, job, taskdesc)
    docker_worker_add_gecko_vcs_env_vars(config, job, taskdesc)
    support_vcs_checkout(config, job, taskdesc)

    env = worker['env']
    env.update({
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'TOOLS_DISABLE': 'true',
        'MOZ_AUTOMATION': '1',
    })

    # tooltool downloads.  By default we download using the API endpoint, but
    # the job can optionally request relengapi-proxy (for example when downloading
    # internal tooltool resources.  So we define the tooltool cache unconditionally.
    worker['caches'].append({
        'type': 'persistent',
        'name': 'tooltool-cache',
        'mount-point': '/home/worker/tooltool-cache',
    })
    env['TOOLTOOL_CACHE'] = '/home/worker/tooltool-cache'

    # tooltool downloads
    worker['relengapi-proxy'] = False  # but maybe enabled for tooltool below
    if run['tooltool-downloads']:
        worker['relengapi-proxy'] = True
        taskdesc['scopes'].extend([
            'docker-worker:relengapi-proxy:tooltool.download.public',
        ])
        if run['tooltool-downloads'] == 'internal':
            taskdesc['scopes'].append(
                'docker-worker:relengapi-proxy:tooltool.download.internal')

    worker['command'] = [
        '/home/worker/bin/run-task',
        # Various caches/volumes are default owned by root:root.
        '--chown-recursive', '/home/worker/workspace',
        '--chown-recursive', '/home/worker/tooltool-cache',
        '--vcs-checkout=/home/worker/workspace/build/src',
        '--',
        'bash',
        '-c',
        'cd /home/worker && '
        './workspace/build/src/taskcluster/scripts/misc/{}'.format(
            run['script'])
    ]

    add_optimizations(config, run, taskdesc)


@run_job_using("generic-worker", "toolchain-script", schema=toolchain_run_schema)
def windows_toolchain(config, job, taskdesc):
    run = job['run']
    taskdesc['run-on-projects'] = ['trunk', 'try']

    worker = taskdesc['worker']

    worker['artifacts'] = [{
        'path': r'public\build',
        'type': 'directory',
    }]

    docker_worker_add_gecko_vcs_env_vars(config, job, taskdesc)

    # We fetch LLVM SVN into this.
    svn_cache = 'level-{}-toolchain-clang-cl-build-svn'.format(config.params['level'])
    worker['mounts'] = [{
        'cache-name': svn_cache,
        'directory': r'llvm-sources',
    }]
    taskdesc['scopes'].extend([
        'generic-worker:cache:' + svn_cache,
    ])

    env = worker['env']
    env.update({
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'MOZ_AUTOMATION': '1',
    })

    hg = r'c:\Program Files\Mercurial\hg.exe'
    hg_command = ['"{}"'.format(hg)]
    hg_command.append('robustcheckout')
    hg_command.extend(['--sharebase', 'y:\\hg-shared'])
    hg_command.append('--purge')
    hg_command.extend(['--upstream', 'https://hg.mozilla.org/mozilla-unified'])
    hg_command.extend(['--revision', '%GECKO_HEAD_REV%'])
    hg_command.append('%GECKO_HEAD_REPOSITORY%')
    hg_command.append('.\\build\\src')

    bash = r'c:\mozilla-build\msys\bin\bash'
    worker['command'] = [
        ' '.join(hg_command),
        # do something intelligent.
        r'{} -c ./build/src/taskcluster/scripts/misc/{}'.format(bash, run['script'])
    ]

    add_optimizations(config, run, taskdesc)
