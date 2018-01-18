# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add from parameters.yml into Balrog publishing tasks.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_release_eta(config, jobs):
    for job in jobs:
        if config.params['release_eta'] != '':
            job['run']['release-eta'] = config.params['release_eta']

        yield job
