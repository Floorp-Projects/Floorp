# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Transforms for adding appropriate scopes to scriptworker tasks.
"""


from gecko_taskgraph.util.scriptworker import get_balrog_server_scope


def add_balrog_scopes(config, jobs):
    for job in jobs:
        server_scope = get_balrog_server_scope(config)
        job["scopes"] = [server_scope]

        yield job
