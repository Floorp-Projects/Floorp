# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Handle notifications like emails.
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy
import json

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import inherit_treeherder_from_dep
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def add_notify_email(config, tasks):
    for task in tasks:
        notify = task.pop('notify', {})
        email_config = notify.get('email')
        if email_config:
            extra = task.setdefault('extra', {})
            notify = extra.setdefault('notify', {})
            notify['email'] = {
                'content': email_config['content'],
                'subject': email_config['subject'],
                'link': email_config.get('link', None),
            }

            routes = task.setdefault('routes', [])
            routes.extend([
                'notify.email.{}.on-{}'.format(address, reason)
                for address in email_config['to-addresses']
                for reason in email_config['on-reasons']
            ])

        yield task
