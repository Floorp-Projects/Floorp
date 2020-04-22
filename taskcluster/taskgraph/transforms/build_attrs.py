# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.platforms import platform_family

transforms = TransformSequence()


@transforms.add
def set_build_attributes(config, jobs):
    """
    Set the build_platform and build_type attributes based on the job name.
    Although not all jobs using this transform are actual "builds", the try
    option syntax treats them as such, and this arranges the attributes
    appropriately for that purpose.
    """
    for job in jobs:
        build_platform, build_type = job['name'].split('/')

        # pgo builds are represented as a different platform, type opt
        if build_type == 'pgo':
            build_platform = build_platform + '-pgo'
            build_type = 'opt'

        attributes = job.setdefault('attributes', {})
        attributes.update({
            'build_platform': build_platform,
            'build_type': build_type,
        })

        yield job


@transforms.add
def set_schedules_optimization(config, jobs):
    """Set the `skip-unless-affected` optimization based on the build platform."""
    for job in jobs:
        # don't add skip-unless-schedules if there's already a when defined
        if 'when' in job:
            yield job
            continue

        build_platform = job['attributes']['build_platform']
        job.setdefault('optimization',
                       {'build': [platform_family(build_platform)]})
        yield job
