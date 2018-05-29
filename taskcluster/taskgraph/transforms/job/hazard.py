# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running hazard jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.schema import Schema
from voluptuous import Required, Optional, Any

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_workspace_cache,
    docker_worker_setup_secrets,
    docker_worker_add_artifacts,
    docker_worker_add_tooltool,
    support_vcs_checkout,
)

haz_run_schema = Schema({
    Required('using'): 'hazard',

    # The command to run within the task image (passed through to the worker)
    Required('command'): basestring,

    # The mozconfig to use; default in the script is used if omitted
    Optional('mozconfig'): basestring,

    # The set of secret names to which the task has access; these are prefixed
    # with `project/releng/gecko/{treeherder.kind}/level-{level}/`.   Setting
    # this will enable any worker features required and set the task's scopes
    # appropriately.  `true` here means ['*'], all secrets.  Not supported on
    # Windows
    Required('secrets', default=False): Any(bool, [basestring]),

    # Base work directory used to set up the task.
    Required('workdir'): basestring,
})


@run_job_using("docker-worker", "hazard", schema=haz_run_schema)
def docker_worker_hazard(config, job, taskdesc):
    run = job['run']

    worker = taskdesc['worker']
    worker['artifacts'] = []

    docker_worker_add_artifacts(config, job, taskdesc)
    docker_worker_add_workspace_cache(config, job, taskdesc)
    docker_worker_add_tooltool(config, job, taskdesc)
    docker_worker_setup_secrets(config, job, taskdesc)
    support_vcs_checkout(config, job, taskdesc)

    env = worker['env']
    env.update({
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
    })

    # script parameters
    if run.get('mozconfig'):
        env['MOZCONFIG'] = run['mozconfig']

    # build-haz-linux.sh needs this otherwise it assumes the checkout is in
    # the workspace.
    env['GECKO_DIR'] = '{workdir}/checkouts/gecko'.format(**run)

    worker['command'] = [
        '{workdir}/bin/run-task'.format(**run),
        '--vcs-checkout', '{workdir}/checkouts/gecko'.format(**run),
        '--',
        '/bin/bash', '-c', run['command']
    ]
