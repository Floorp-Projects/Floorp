# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This transform is used to help populate mozharness options for openh264 jobs
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def set_mh_options(config, jobs):
    """
    This transform enables coalescing for tasks with scm level > 1, setting the
    name to be equal to the the first 20 chars of the sha256 of the task name
    (label). The hashing is simply used to keep the name short, unique, and
    alphanumeric, since it is used in AMQP routing keys.

    Note, the coalescing keys themselves are not intended to be readable
    strings or embed information. The tasks they represent contain all relevant
    metadata.
    """
    for job in jobs:
        repo = job.pop('repo')
        rev = job.pop('revision')
        attributes = job.setdefault('attributes', {})
        attributes['openh264_rev'] = rev
        run = job.setdefault('run', {})
        options = run.setdefault('options', [])
        options.extend(['repo={}'.format(repo), 'rev={}'.format(rev)])
        yield job
