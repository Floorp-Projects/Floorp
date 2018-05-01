# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add notifications via taskcluster-notify for release tasks
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.scriptworker import get_release_config, RELEASE_NOTIFICATION_PHASES
from taskgraph.util.schema import resolve_keyed_by


transforms = TransformSequence()

EMAIL_DESTINATIONS = {
    'mozilla-beta': ["release-automation-notifications@mozilla.com"],
    'mozilla-release': ["release-automation-notifications@mozilla.com"],
    'mozilla-esr60': ["release-automation-notifications@mozilla.com"],
    # otherwise []
}

# Only notify on tasks that have issues
DEFAULT_ROUTES = [
    'notify.email.{email_dest}.on-failed',
    'notify.email.{email_dest}.on-exception',
]

SUBJECT_TEMPLATE = "${{status.state}}: [{shipping_product} {release_config[version]} " + \
                   "build{release_config[build_number]}/{config[params][project]}] {label}"


@transforms.add
def add_notifications(config, jobs):
    release_config = get_release_config(config)
    email_dest = EMAIL_DESTINATIONS.get(config.params['project'], [])

    for job in jobs:
        # Frankly, my dear, you're all over the place
        shipping_phase = job.get('attributes', {}).get('shipping_phase') or \
            job.get('shipping-phase')
        shipping_product = job.get('attributes', {}).get('shipping_product') or \
            job.get('shipping-product')
        label = job.get('label') or '{}-{}'.format(config.kind, job['name'])

        # Handle notification overrides
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
            # we only send these on succces to avoid messages like 'blah is in the
            # candidates dir' when cancelling graphs, dummy job failure, etc
            routes = ['notify.email.{email_dest}.on-success']
            # Don't need this any more
            del job['notifications']
        else:
            emails = email_dest
            format_kwargs = dict(
                label=label,
                shipping_product=shipping_product,
                config=config.__dict__,
                release_config=release_config,
            )
            subject = SUBJECT_TEMPLATE.format(**format_kwargs)
            message = None
            routes = DEFAULT_ROUTES

        # We only modify release jobs, or nightly & release being run in the context of a release
        if shipping_phase in RELEASE_NOTIFICATION_PHASES and \
                config.params['target_tasks_method'].startswith(RELEASE_NOTIFICATION_PHASES):

            # Add routes to trigger notifications via tc-notify
            for dest in emails:
                job.setdefault('routes', []).extend(
                    [r.format(email_dest=dest) for r in routes]
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
