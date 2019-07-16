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
        # Replace the worker attribute `wrupdater-secrets: true` with scopes
        # to access the github token, but only on m-c. Doing this on any other
        # tree will result in decision task failure because m-c is the only one
        # allowed to have that scope.
        worker = task['worker']
        if worker.get('wrupdater-secrets', False):
            del worker['wrupdater-secrets']
            if config.params['project'] == 'mozilla-central':
                task.setdefault('scopes', [])
                task['scopes'].append('secrets:get:project/webrender-ci/wrupdater-github-token')
            elif config.params['project'] != 'try':
                # Discard this task on all trees other than m-c and try,
                # otherwise it will fail.
                continue
        yield task
