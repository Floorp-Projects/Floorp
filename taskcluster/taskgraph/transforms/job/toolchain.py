# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running toolchain-building jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

import time
from voluptuous import Schema, Required

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_tc_vcs_cache,
    docker_worker_add_gecko_vcs_env_vars
)

toolchain_run_schema = Schema({
    Required('using'): 'toolchain-script',

    # the script (in taskcluster/scripts/misc) to run
    Required('script'): basestring,
})


@run_job_using("docker-worker", "toolchain-script", schema=toolchain_run_schema)
def docker_worker_toolchain(config, job, taskdesc):
    run = job['run']

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

    env = worker['env']
    env.update({
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'TOOLS_DISABLE': 'true',
    })

    # tooltool downloads; note that this downloads using the API endpoint directly,
    # rather than via relengapi-proxy
    worker['caches'].append({
        'type': 'persistent',
        'name': 'tooltool-cache',
        'mount-point': '/home/worker/tooltool-cache',
    })
    env['TOOLTOOL_CACHE'] = '/home/worker/tooltool-cache'
    env['TOOLTOOL_REPO'] = 'https://github.com/mozilla/build-tooltool'
    env['TOOLTOOL_REV'] = 'master'

    command = ' && '.join([
        "cd /home/worker/",
        "./bin/checkout-sources.sh",
        "./workspace/build/src/taskcluster/scripts/misc/" + run['script'],
    ])
    worker['command'] = ["/bin/bash", "-c", command]


@run_job_using("generic-worker", "toolchain-script", schema=toolchain_run_schema)
def windows_toolchain(config, job, taskdesc):
    run = job['run']

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
        'path': r'llvm-sources',
    }]
    taskdesc['scopes'].extend([
        'generic-worker:cache:' + svn_cache,
    ])

    env = worker['env']
    env.update({
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'TOOLTOOL_REPO': 'https://github.com/mozilla/build-tooltool',
        'TOOLTOOL_REV': 'master',
    })

    hg = r'c:\Program Files\Mercurial\hg.exe'
    bash = r'c:\mozilla-build\msys\bin\bash'
    worker['command'] = [
        r'mkdir .\build\src',
        r'"{}" share c:\builds\hg-shared\mozilla-central .\build\src'.format(hg),
        r'"{}" pull -u -R .\build\src --rev %GECKO_HEAD_REV% %GECKO_HEAD_REPOSITORY%'.format(hg),
        # do something intelligent.
        r'{} -c ./build/src/taskcluster/scripts/misc/{}'.format(bash, run['script'])
    ]
