# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

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
        if '/' in job['name']:
            build_platform, build_type = job['name'].split('/')
        else:
            build_platform = job['name']
            build_type = 'opt'

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
