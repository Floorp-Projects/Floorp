# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running phone-builder tasks via the phone-builder script.
"""

from __future__ import absolute_import, print_function, unicode_literals

import time
from voluptuous import Schema, Required

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_gecko_vcs_env_vars,
    docker_worker_add_tc_vcs_cache
)

phone_builder_linux_schema = Schema({
    Required('using'): 'phone-builder',

    # if true, this is a debug run
    Required('debug', default=False): bool,

    # the TARGET to run
    Required('target'): basestring,
})


@run_job_using("docker-worker", "phone-builder", schema=phone_builder_linux_schema)
def docker_worker_phone_builder(config, job, taskdesc):
    run = job['run']
    worker = taskdesc.get('worker')

    worker['artifacts'] = [{
        'name': 'private/build',
        'path': '/home/worker/artifacts/',
        'type': 'directory',
    }, {
        'name': 'public/build',
        'path': '/home/worker/artifacts-public/',
        'type': 'directory',
    }]

    docker_worker_add_tc_vcs_cache(config, job, taskdesc)
    docker_worker_add_gecko_vcs_env_vars(config, job, taskdesc)

    if config.params['project'] != 'try':
        taskdesc['worker']['caches'].append({
            'type': 'persistent',
            'name': 'level-{}-{}-build-{}-{}'.format(
                config.params['level'], config.params['project'],
                taskdesc['attributes']['build_platform'],
                taskdesc['attributes']['build_type'],),
            'mount-point': "/home/worker/workspace",
        })
        taskdesc['worker']['caches'].append({
            'type': 'persistent', 'name':
            'level-{}-{}-build-{}-{}-objdir-gecko'.format(
                config.params['level'], config.params['project'],
                taskdesc['attributes']['build_platform'],
                taskdesc['attributes']['build_type'],),
            'mount-point': "/home/worker/objdir-gecko",
        })

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_CONFIG': 'b2g/taskcluster-phone-eng.py',
        'MOZ_BUILD_DATE': time.strftime("%Y%m%d%H%M%S", time.gmtime(config.params['pushdate'])),
        'TARGET': run['target'],
    })
    if run['debug']:
        env['B2G_DEBUG'] = '1'

    # tooltool downloads
    worker['relengapi-proxy'] = True
    taskdesc['scopes'].extend([
        'docker-worker:relengapi-proxy:tooltool.download.internal',
        'docker-worker:relengapi-proxy:tooltool.download.public',
    ])

    worker['command'] = [
        "/bin/bash",
        "-c",
        "checkout-gecko workspace"
        " && cd ./workspace/gecko/taskcluster/scripts/phone-builder"
        " && buildbot_step 'Build' ./build-phone.sh $HOME/workspace",
    ]
