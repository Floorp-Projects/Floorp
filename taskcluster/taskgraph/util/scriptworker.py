# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""Make scriptworker.cot.verify more user friendly by making scopes dynamic.

Scriptworker uses certain scopes to determine which sets of credentials to use.
Certain scopes are restricted by branch in chain of trust verification, and are
checked again at the script level.  This file provides functions to adjust
these scopes automatically by project; this makes pushing to try, forking a
project branch, and merge day uplifts more user friendly.

In the future, we may adjust scopes by other settings as well, e.g. different
scopes for `push-to-candidates` rather than `push-to-releases`, even if both
happen on mozilla-beta and mozilla-release.
"""
from __future__ import absolute_import, print_function, unicode_literals
from copy import deepcopy
import functools


"""Map signing scope aliases to sets of projects.

Currently m-c and m-a use nightly signing; m-b and m-r use release signing.
We will need to add esr support at some point. Eventually we want to add
nuance so certain m-b and m-r tasks use dep or nightly signing, and we only
release sign when we have a signed-off set of candidate builds.  This current
approach works for now, though.

This is a list of list-pairs, for ordering.
"""
SIGNING_SCOPE_ALIAS_TO_PROJECT = [[
    'all-nightly-branches', set([
        'mozilla-central',
        'mozilla-aurora',
    ])
], [
    'all-release-branches', set([
        'mozilla-beta',
        'mozilla-release',
    ])
]]

"""Map the signing scope aliases to the actual scopes.
"""
SIGNING_CERT_SCOPES = {
    'all-release-branches': 'project:releng:signing:cert:release-signing',
    'all-nightly-branches': 'project:releng:signing:cert:nightly-signing',
    'default': 'project:releng:signing:cert:dep-signing',
}

"""Map beetmover scope aliases to sets of projects.

Currently this mirrors the signing scope alias behavior.
"""
BEETMOVER_SCOPE_ALIAS_TO_PROJECT = deepcopy(SIGNING_SCOPE_ALIAS_TO_PROJECT)

"""Map the beetmover scope aliases to the actual scopes.
"""
BEETMOVER_BUCKET_SCOPES = {
    'all-release-branches': 'project:releng:beetmover:bucket:release',
    'all-nightly-branches': 'project:releng:beetmover:bucket:nightly',
    'default': 'project:releng:beetmover:bucket:dep',
}

"""Map balrog scope aliases to sets of projects.

This is a list of list-pairs, for ordering.
"""
BALROG_SCOPE_ALIAS_TO_PROJECT = [[
    'nightly', set([
        'mozilla-central',
    ])
], [
    'aurora', set([
        'mozilla-aurora',
    ])
], [
    'beta', set([
        'mozilla-beta',
    ])
], [
    'release', set([
        'mozilla-release',
    ])
]]

"""Map the balrog scope aliases to the actual scopes.
"""
BALROG_SERVER_SCOPES = {
    'nightly': 'project:releng:balrog:server:nightly',
    'aurora': 'project:releng:balrog:server:aurora',
    'beta': 'project:releng:balrog:server:beta',
    'release': 'project:releng:balrog:server:release',
    'default': 'project:releng:balrog:server:dep',
}


def get_scope_from_project(alias_to_project_map, alias_to_scope_map, config):
    """Determine the restricted scope from `config.params['project']`.

    Args:
        alias_to_project_map (list of lists): each list pair contains the
            alias alias and the set of projects that match.  This is ordered.
        alias_to_scope_map (dict): the alias alias to scope
        config (dict): the task config that defines the project.

    Returns:
        string: the scope to use.
    """
    for alias, projects in alias_to_project_map:
        if config.params['project'] in projects and alias in alias_to_scope_map:
            return alias_to_scope_map[alias]
    return alias_to_scope_map['default']


get_signing_cert_scope = functools.partial(
    get_scope_from_project,
    SIGNING_SCOPE_ALIAS_TO_PROJECT,
    SIGNING_CERT_SCOPES
)

get_beetmover_bucket_scope = functools.partial(
    get_scope_from_project,
    BEETMOVER_SCOPE_ALIAS_TO_PROJECT,
    BEETMOVER_BUCKET_SCOPES
)

get_balrog_server_scope = functools.partial(
    get_scope_from_project,
    BALROG_SCOPE_ALIAS_TO_PROJECT,
    BALROG_SERVER_SCOPES
)
