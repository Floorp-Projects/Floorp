# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running mach tasks (via run-task)
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.run_task import (
    docker_worker_run_task,
    native_engine_run_task,
)
from taskgraph.util.schema import Schema
from voluptuous import Required

mach_schema = Schema({
    Required('using'): 'mach',

    # The mach command (omitting `./mach`) to run
    Required('mach'): basestring,

    # Whether the job requires a build artifact or not. If True, the task
    # will depend on a build task and run-task will download and set up the
    # installer. Build labels are determined by the `dependent-build-platforms`
    # config in kind.yml.
    Required('requires-build', default=False): bool,
})


@run_job_using("docker-worker", "mach", schema=mach_schema)
@run_job_using("native-engine", "mach", schema=mach_schema)
def docker_worker_mach(config, job, taskdesc):
    run = job['run']

    # defer to the run_task implementation
    run['command'] = 'cd ~/checkouts/gecko && ./mach ' + run['mach']
    run['checkout'] = True
    del run['mach']
    if job['worker']['implementation'] == 'docker-worker':
        docker_worker_run_task(config, job, taskdesc)
    else:
        native_engine_run_task(config, job, taskdesc)
