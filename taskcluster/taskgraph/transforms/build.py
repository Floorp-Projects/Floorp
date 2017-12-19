# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.workertypes import worker_type_implementation

transforms = TransformSequence()


@transforms.add
def set_defaults(config, jobs):
    """Set defaults, including those that differ per worker implementation"""
    for job in jobs:
        job['treeherder'].setdefault('kind', 'build')
        job['treeherder'].setdefault('tier', 1)
        _, worker_os = worker_type_implementation(job['worker-type'])
        worker = job.setdefault('worker', {})
        worker.setdefault('env', {})
        if worker_os == "linux":
            worker.setdefault('docker-image', {'in-tree': 'desktop-build'})
            worker['chain-of-trust'] = True
            extra = job.setdefault('extra', {})
            extra.setdefault('chainOfTrust', {})
            extra['chainOfTrust'].setdefault('inputs', {})
            if 'in-tree' in worker['docker-image']:
                extra['chainOfTrust']['inputs']['docker-image'] = {
                    "task-reference": "<docker-image>"
                }
        elif worker_os == "windows":
            worker['chain-of-trust'] = True

        yield job


@transforms.add
def set_env(config, jobs):
    """Set extra environment variables from try command line."""
    env = {}
    if config.params['try_mode'] == 'try_option_syntax':
        env = config.params['try_options']['env'] or {}
    for job in jobs:
        if env:
            job['worker']['env'].update(dict(x.split('=') for x in env))
        yield job
