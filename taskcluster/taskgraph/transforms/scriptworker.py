# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Transforms for adding appropriate scopes to scriptworker tasks.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.scriptworker import (
    get_balrog_action_scope,
    get_balrog_server_scope,
    get_worker_type_for_scope,
)


def add_balrog_scopes(config, jobs):
    for job in jobs:
        worker = job['worker']

        server_scope = get_balrog_server_scope(config)
        action_scope = get_balrog_action_scope(config, action=worker['balrog-action'])
        job['scopes'] = [server_scope, action_scope]

        job['worker-type'] = get_worker_type_for_scope(config, server_scope)

        yield job
