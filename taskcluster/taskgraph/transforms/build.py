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
        job.setdefault('needs-sccache', True)
        _, worker_os = worker_type_implementation(job['worker-type'])
        if worker_os == "linux":
            worker = job.setdefault('worker')
            worker.setdefault('docker-image', {'in-tree': 'desktop-build'})
            worker['chain-of-trust'] = True
            extra = job.setdefault('extra', {})
            extra.setdefault('chainOfTrust', {})
            extra['chainOfTrust'].setdefault('inputs', {})
            extra['chainOfTrust']['inputs']['docker-image'] = {
                "task-reference": "<docker-image>"
            }
        elif worker_os in set(["macosx", "windows"]):
            job['worker'].setdefault('env', {})
        yield job


@transforms.add
def set_env(config, jobs):
    """Set extra environment variables from try command line."""
    for job in jobs:
        env = config.config['args'].env
        if env:
            job_env = {}
            if 'worker' in job:
                job_env = job['worker']['env']
            job_env.update(dict(x.split('=') for x in env))
        yield job
