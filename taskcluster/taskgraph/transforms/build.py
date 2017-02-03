# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def set_defaults(config, jobs):
    """Set defaults, including those that differ per worker implementation"""
    for job in jobs:
        job['treeherder'].setdefault('kind', 'build')
        job['treeherder'].setdefault('tier', 1)
        job.setdefault('needs-sccache', True)
        if job['worker']['implementation'] in ('docker-worker', 'docker-engine'):
            job['worker'].setdefault('docker-image', {'in-tree': 'desktop-build'})
            job['worker']['chain-of-trust'] = True
            job.setdefault('extra', {})
            job['extra'].setdefault('chainOfTrust', {})
            job['extra']['chainOfTrust'].setdefault('inputs', {})
            job['extra']['chainOfTrust']['inputs']['docker-image'] = {
                "task-reference": "<docker-image>"
            }
        job['worker'].setdefault('env', {})
        yield job


@transforms.add
def set_env(config, jobs):
    """Set extra environment variables from try command line."""
    for job in jobs:
        env = config.config['args'].env
        if env:
            job_env = job['worker']['env']
            job_env.update(dict(x.split('=') for x in env))
        yield job
