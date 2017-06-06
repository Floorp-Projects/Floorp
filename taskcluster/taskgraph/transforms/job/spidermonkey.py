# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running spidermonkey jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.schema import Schema
from voluptuous import Required, Optional, Any

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_public_artifacts,
    support_vcs_checkout,
)

sm_run_schema = Schema({
    Required('using'): Any('spidermonkey', 'spidermonkey-package', 'spidermonkey-mozjs-crate'),

    # The SPIDERMONKEY_VARIANT
    Required('spidermonkey-variant'): basestring,

    # The tooltool manifest to use; default from sm-tooltool-config.sh  is used
    # if omitted
    Optional('tooltool-manifest'): basestring,
})


@run_job_using("docker-worker", "spidermonkey", schema=sm_run_schema)
@run_job_using("docker-worker", "spidermonkey-package", schema=sm_run_schema)
@run_job_using("docker-worker", "spidermonkey-mozjs-crate",
               schema=sm_run_schema)
def docker_worker_spidermonkey(config, job, taskdesc):
    run = job['run']

    worker = taskdesc['worker']
    worker['artifacts'] = []
    worker['caches'] = []

    if int(config.params['level']) > 1:
        worker['caches'].append({
            'type': 'persistent',
            'name': 'level-{}-{}-build-spidermonkey-workspace'.format(
                config.params['level'], config.params['project']),
            'mount-point': "/home/worker/workspace",
        })

    docker_worker_add_public_artifacts(config, job, taskdesc)

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_DISABLE': 'true',
        'SPIDERMONKEY_VARIANT': run['spidermonkey-variant'],
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
    })

    # tooltool downloads; note that this script downloads using the API
    # endpoiint directly, rather than via relengapi-proxy
    worker['caches'].append({
        'type': 'persistent',
        'name': 'tooltool-cache',
        'mount-point': '/home/worker/tooltool-cache',
    })
    env['TOOLTOOL_CACHE'] = '/home/worker/tooltool-cache'
    if run.get('tooltool-manifest'):
        env['TOOLTOOL_MANIFEST'] = run['tooltool-manifest']

    support_vcs_checkout(config, job, taskdesc)

    script = "build-sm.sh"
    if run['using'] == 'spidermonkey-package':
        script = "build-sm-package.sh"
    elif run['using'] == 'spidermonkey-mozjs-crate':
        script = "build-sm-mozjs-crate.sh"

    worker['command'] = [
        '/home/worker/bin/run-task',
        '--chown-recursive', '/home/worker/workspace',
        '--chown-recursive', '/home/worker/tooltool-cache',
        '--vcs-checkout', '/home/worker/workspace/build/src',
        '--',
        '/bin/bash',
        '-c',
        'cd /home/worker && workspace/build/src/taskcluster/scripts/builder/%s' % script
    ]
