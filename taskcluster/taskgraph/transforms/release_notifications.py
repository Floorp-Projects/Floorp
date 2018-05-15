# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add notifications via taskcluster-notify for release tasks
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.scriptworker import get_release_config
from taskgraph.util.schema import resolve_keyed_by


transforms = TransformSequence()


@transforms.add
def add_notifications(config, jobs):
    release_config = get_release_config(config)

    for job in jobs:
        label = '{}-{}'.format(config.kind, job['name'])

        notifications = job.get('notifications')
        if notifications:
            resolve_keyed_by(notifications, 'emails', label, project=config.params['project'])
            emails = notifications['emails']
            format_kwargs = dict(
                task=job,
                config=config.__dict__,
                release_config=release_config,
            )
            subject = notifications['subject'].format(**format_kwargs)
            message = notifications['message'].format(**format_kwargs)
            emails = [email.format(**format_kwargs) for email in emails]
            # Don't need this any more
            del job['notifications']

            # We only send mail on success to avoid messages like 'blah is in the
            # candidates dir' when cancelling graphs, dummy job failure, etc
            job.setdefault('routes', []).extend(
                ['notify.email.{}.on-completed'.format(email) for email in emails]
            )

            # Customize the email subject to include release name and build number
            job.setdefault('extra', {}).update(
                {
                   'notify': {
                       'email': {
                            'subject': subject,
                        }
                    }
                }
            )
            if message:
                job['extra']['notify']['email']['content'] = message

        yield job
