# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running mulet tasks via build-mulet-linux.sh
"""

from __future__ import absolute_import, print_function, unicode_literals

import time
from voluptuous import Schema, Required

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_workspace_cache,
    docker_worker_add_tc_vcs_cache,
    docker_worker_add_gecko_vcs_env_vars,
    docker_worker_add_public_artifacts
)

COALESCE_KEY = 'builds.{project}.{name}'

build_mulet_linux_schema = Schema({
    Required('using'): 'mach-via-build-mulet-linux.sh',

    # The pathname of the mozconfig to use
    Required('mozconfig'): basestring,

    # The tooltool manifest to use
    Required('tooltool-manifest'): basestring,
})


@run_job_using("docker-worker", "mach-via-build-mulet-linux.sh", schema=build_mulet_linux_schema)
def docker_worker_make_via_build_mulet_linux_sh(config, job, taskdesc):
    run = job['run']
    worker = taskdesc.get('worker')

    # assumes the builder image (which contains the gecko checkout command)
    taskdesc['worker']['docker-image'] = {"in-tree": "builder"}

    worker['taskcluster-proxy'] = False

    docker_worker_add_public_artifacts(config, job, taskdesc)
    docker_worker_add_tc_vcs_cache(config, job, taskdesc)
    docker_worker_add_workspace_cache(config, job, taskdesc)
    docker_worker_add_gecko_vcs_env_vars(config, job, taskdesc)

    env = worker.setdefault('env', {})
    env.update({
        'MOZ_BUILD_DATE': time.strftime("%Y%m%d%H%M%S", time.gmtime(config.params['pushdate'])),
        'MOZ_SCM_LEVEL': config.params['level'],
    })

    env['MOZCONFIG'] = run['mozconfig']

    # tooltool downloads (not via relengapi proxy)
    worker['caches'].append({
        'type': 'persistent',
        'name': 'tooltool-cache',
        'mount-point': '/home/worker/tooltool-cache',
    })
    env['TOOLTOOL_CACHE'] = '/home/worker/tooltool-cache'
    env['TOOLTOOL_MANIFEST'] = run['tooltool-manifest']
    env['TOOLTOOL_REPO'] = 'https://github.com/mozilla/build-tooltool'
    env['TOOLTOOL_REV'] = 'master'

    worker['command'] = [
        "/bin/bash",
        "-c",
        "checkout-gecko workspace"
        " && cd ./workspace/gecko/taskcluster/scripts/builder"
        " && buildbot_step 'Build' ./build-mulet-linux.sh $HOME/workspace",
    ]

mulet_simulator_schema = Schema({
    Required('using'): 'mulet-simulator',

    # The shell command to run with `bash -exc`.  This will have parameters
    # substituted for {..} and will be enclosed in a {task-reference: ..} block
    # so it can refer to the parent task as <build>
    Required('shell-command'): basestring,
})


@run_job_using("docker-worker", "mulet-simulator", schema=mulet_simulator_schema)
def docker_worker_mulet_simulator(config, job, taskdesc):
    run = job['run']
    worker = taskdesc.get('worker')

    # assumes the builder image (which contains the gecko checkout command)
    taskdesc['worker']['docker-image'] = {"in-tree": "builder"}

    worker['taskcluster-proxy'] = False

    docker_worker_add_public_artifacts(config, job, taskdesc)
    docker_worker_add_gecko_vcs_env_vars(config, job, taskdesc)

    shell_command = run['shell-command'].format(**config.params)

    worker['command'] = [
        "/bin/bash",
        "-exc",
        {'task-reference': shell_command},
    ]
