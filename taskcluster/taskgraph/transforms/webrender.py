# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def webrender(config, tasks):
    """Do transforms specific to webrender tasks.
    """
    for task in tasks:
        # Replace the worker attribute wrupdater-secrets: true with permissions
        # to access the github token, but only tier-3 trees (i.e. m-c and
        # integration branches).
        worker = task['worker']
        if worker.get('wrupdater-secrets', False):
            del worker['wrupdater-secrets']
            if int(config.params['level']) == 3:
                task.setdefault('scopes', [])
                task['scopes'].append('secrets:get:project/webrender-ci/wrupdater-github-token')
        yield task
