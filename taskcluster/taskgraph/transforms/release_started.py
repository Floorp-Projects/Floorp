# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add notifications via taskcluster-notify for release tasks
"""

from __future__ import absolute_import, print_function, unicode_literals

import os
from pipes import quote as shell_quote

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by


transforms = TransformSequence()


@transforms.add
def add_notifications(config, jobs):
    for job in jobs:
        label = '{}-{}'.format(config.kind, job['name'])

        resolve_keyed_by(job, 'emails', label, project=config.params['project'])
        emails = [email.format(config=config.__dict__) for email in job.pop('emails')]

        command = [
            'release',
            'send-buglist-email',
            '--version', config.params['version'],
            '--product', job['shipping-product'],
            '--revision', config.params['head_rev'],
            '--build-number', str(config.params['build_number']),
            '--repo', config.params['head_repository'],
        ]
        for address in emails:
            command += ['--address', address]
        if 'TASK_ID' in os.environ:
            command += [
                '--task-group-id', os.environ['TASK_ID'],
            ]

        job['scopes'] = ['notify:email:{}'.format(address) for address in emails]
        job['run'] = {
            'using': 'mach',
            'sparse-profile': 'mach',
            'mach': ' '.join(map(shell_quote, command)),
        }

        yield job
