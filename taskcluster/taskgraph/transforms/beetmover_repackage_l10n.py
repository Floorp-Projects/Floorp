# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import join_symbol

transforms = TransformSequence()


@transforms.add
def make_beetmover_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        locale = dep_job.attributes.get('locale')
        if not locale:
            yield job
            continue

        group = 'tc-BMR-L10n'

        # add the locale code
        symbol = locale

        treeherder = {
            'symbol': join_symbol(group, symbol),
        }

        beet_description = {
            'label': job['label'],
            'dependent-task': dep_job,
            'treeherder': treeherder,
            'locale': locale,
            'shipping-phase': job.get('shipping-phase'),
            'shipping-product': job.get('shipping-product'),
        }
        yield beet_description
