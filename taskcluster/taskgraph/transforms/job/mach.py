# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running mach tasks (via run-task)
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.job import run_job_using, configure_taskdesc_for_run
from taskgraph.util.schema import (
    Schema,
    taskref_or_string,
)
from voluptuous import Required, Optional, Any

mach_schema = Schema({
    Required('using'): 'mach',

    # The mach command (omitting `./mach`) to run
    Required('mach'): taskref_or_string,

    # The sparse checkout profile to use. Value is the filename relative to the
    # directory where sparse profiles are defined (build/sparse-profiles/).
    Optional('sparse-profile'): Any(basestring, None),

    # if true, perform a checkout of a comm-central based branch inside the
    # gecko checkout
    Required('comm-checkout'): bool,

    # Base work directory used to set up the task.
    Required('workdir'): basestring,
})


defaults = {
    'comm-checkout': False,
}


@run_job_using("docker-worker", "mach", schema=mach_schema, defaults=defaults)
@run_job_using("generic-worker", "mach", schema=mach_schema, defaults=defaults)
def configure_mach(config, job, taskdesc):
    run = job['run']

    command_prefix = 'cd $GECKO_PATH && ./mach '
    if job['worker-type'].endswith('1014'):
        command_prefix = 'cd $GECKO_PATH && LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8 ./mach '

    mach = run['mach']
    if isinstance(mach, dict):
        ref, pattern = next(iter(mach.items()))
        command = {ref: command_prefix + pattern}
    else:
        command = command_prefix + mach

    # defer to the run_task implementation
    run['command'] = command
    run['using'] = 'run-task'
    del run['mach']
    configure_taskdesc_for_run(config, job, taskdesc, job['worker']['implementation'])
