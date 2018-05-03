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

Additional configuration is found in the :ref:`graph config <taskgraph-graph-config>`.
"""
from __future__ import absolute_import, print_function, unicode_literals
import functools
import json
import os


# constants {{{1
"""Map signing scope aliases to sets of projects.

Currently m-c and DevEdition on m-b use nightly signing; Beta on m-b and m-r
use release signing. These data structures aren't set-up to handle different
scopes on the same repo, so we use a different set of them for DevEdition, and
callers are responsible for using the correct one (by calling the appropriate
helper below). More context on this in https://bugzilla.mozilla.org/show_bug.cgi?id=1358601.

We will need to add esr support at some point. Eventually we want to add
nuance so certain m-b and m-r tasks use dep or nightly signing, and we only
release sign when we have a signed-off set of candidate builds.  This current
approach works for now, though.

This is a list of list-pairs, for ordering.
"""
SIGNING_SCOPE_ALIAS_TO_PROJECT = [[
    'all-nightly-branches', set([
        'mozilla-central',
        'comm-central',
    ])
], [
    'all-release-branches', set([
        'mozilla-beta',
        'mozilla-release',
        'comm-beta',
    ])
]]

"""Map the signing scope aliases to the actual scopes.
"""
SIGNING_CERT_SCOPES = {
    'all-release-branches': 'signing:cert:release-signing',
    'all-nightly-branches': 'signing:cert:nightly-signing',
    'default': 'signing:cert:dep-signing',
}

DEVEDITION_SIGNING_SCOPE_ALIAS_TO_PROJECT = [[
    'beta', set([
        'mozilla-beta',
    ])
]]

DEVEDITION_SIGNING_CERT_SCOPES = {
    'beta': 'signing:cert:nightly-signing',
    'default': 'signing:cert:dep-signing',
}

"""Map beetmover scope aliases to sets of projects.
"""
BEETMOVER_SCOPE_ALIAS_TO_PROJECT = [[
    'all-nightly-branches', set([
        'mozilla-central',
        'mozilla-beta',
        'mozilla-release',
        'comm-central',
    ])
], [
    'all-release-branches', set([
        'mozilla-beta',
        'mozilla-release',
    ])
]]

"""The set of all beetmover release target tasks.

Used for both `BEETMOVER_SCOPE_ALIAS_TO_TARGET_TASK` and `get_release_build_number`
"""
BEETMOVER_CANDIDATES_TARGET_TASKS = set([
    'promote_fennec',
    'promote_desktop',
])
BEETMOVER_PUSH_TARGET_TASKS = set([
    'push_fennec',
    'ship_fennec',
    'push_desktop',
    'ship_desktop',
])
BEETMOVER_RELEASE_TARGET_TASKS = BEETMOVER_CANDIDATES_TARGET_TASKS | BEETMOVER_PUSH_TARGET_TASKS

"""Map beetmover tasks aliases to sets of target task methods.

This is a list of list-pairs, for ordering.
"""
BEETMOVER_SCOPE_ALIAS_TO_TARGET_TASK = [[
    'all-nightly-tasks', set([
        'nightly_fennec',
        'nightly_linux',
        'nightly_macosx',
        'nightly_win32',
        'nightly_win64',
        'nightly_desktop',
        'mozilla_beta_tasks',
        'mozilla_release_tasks',
    ])
], [
    'all-candidates-tasks', BEETMOVER_CANDIDATES_TARGET_TASKS
], [
    'all-push-tasks', BEETMOVER_PUSH_TARGET_TASKS
]]

"""Map the beetmover scope aliases to the actual scopes.
"""
BEETMOVER_BUCKET_SCOPES = {
    'all-candidates-tasks': {
        'all-release-branches': 'beetmover:bucket:release',
    },
    'all-push-tasks': {
        'all-release-branches': 'beetmover:bucket:release',
    },
    'all-nightly-tasks': {
        'all-nightly-branches': 'beetmover:bucket:nightly',
    },
    'default': 'beetmover:bucket:dep',
}

"""Map the beetmover tasks aliases to the actual action scopes.
"""
BEETMOVER_ACTION_SCOPES = {
    'all-candidates-tasks': 'beetmover:action:push-to-candidates',
    'all-push-tasks': 'beetmover:action:push-to-releases',
    'all-nightly-tasks': 'beetmover:action:push-to-nightly',
    'default': 'beetmover:action:push-to-nightly',
}


"""Map the beetmover tasks aliases to phases.
"""
PHASES = {
    'all-candidates-tasks': 'promote',
    'all-push-tasks': 'push',
    'default': None,
}

"""Known balrog actions."""
BALROG_ACTIONS = ('submit-locale', 'submit-toplevel', 'schedule')

"""Map balrog scope aliases to sets of projects.

This is a list of list-pairs, for ordering.
"""
BALROG_SCOPE_ALIAS_TO_PROJECT = [[
    'nightly', set([
        'mozilla-central',
        'comm-central'
    ])
], [
    'beta', set([
        'mozilla-beta',
    ])
], [
    'release', set([
        'mozilla-release',
    ])
], [
    'esr', set([
        'mozilla-esr52',
    ])
]]

"""Map the balrog scope aliases to the actual scopes.
"""
BALROG_SERVER_SCOPES = {
    'nightly': 'balrog:server:nightly',
    'aurora': 'balrog:server:aurora',
    'beta': 'balrog:server:beta',
    'release': 'balrog:server:release',
    'esr': 'balrog:server:esr',
    'default': 'balrog:server:dep',
}


PUSH_APK_SCOPE_ALIAS_TO_PROJECT = [[
    'central', set([
        'mozilla-central',
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


PUSH_APK_SCOPES = {
    'central': 'googleplay:aurora',
    'beta': 'googleplay:beta',
    'release': 'googleplay:release',
    'default': 'googleplay:invalid',
}


""" The list of the release promotion phases which we send notifications for
"""
RELEASE_NOTIFICATION_PHASES = ('promote', 'push', 'ship')


def add_scope_prefix(config, scope):
    """
    Prepends the scriptworker scope prefix from the :ref:`graph config
    <taskgraph-graph-config>`.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        scope (string): The suffix of the scope

    Returns:
        string: the scope to use.
    """
    return "{prefix}:{scope}".format(
        prefix=config.graph_config['scriptworker']['scope-prefix'],
        scope=scope,
    )


def with_scope_prefix(f):
    """
    Wraps a function, calling :py:func:`add_scope_prefix` on the result of
    calling the wrapped function.

    Args:
        f (callable): A function that takes a ``config`` and some keyword
            arguments, and returns a scope suffix.

    Returns:
        callable: the wrapped function
    """
    @functools.wraps(f)
    def wrapper(config, **kwargs):
        scope_or_scopes = f(config, **kwargs)
        if isinstance(scope_or_scopes, list):
            return map(functools.partial(add_scope_prefix, config), scope_or_scopes)
        else:
            return add_scope_prefix(config, scope_or_scopes)

    return wrapper


# scope functions {{{1
@with_scope_prefix
def get_scope_from_project(config, alias_to_project_map, alias_to_scope_map):
    """Determine the restricted scope from `config.params['project']`.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        alias_to_project_map (list of lists): each list pair contains the
            alias and the set of projects that match.  This is ordered.
        alias_to_scope_map (dict): the alias alias to scope

    Returns:
        string: the scope to use.
    """
    for alias, projects in alias_to_project_map:
        if config.params['project'] in projects and alias in alias_to_scope_map:
            return alias_to_scope_map[alias]
    return alias_to_scope_map['default']


@with_scope_prefix
def get_scope_from_target_method(config, alias_to_tasks_map, alias_to_scope_map):
    """Determine the restricted scope from `config.params['target_tasks_method']`.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        alias_to_tasks_map (list of lists): each list pair contains the
            alias and the set of target methods that match. This is ordered.
        alias_to_scope_map (dict): the alias alias to scope

    Returns:
        string: the scope to use.
    """
    for alias, tasks in alias_to_tasks_map:
        if config.params['target_tasks_method'] in tasks and alias in alias_to_scope_map:
            return alias_to_scope_map[alias]
    return alias_to_scope_map['default']


@with_scope_prefix
def get_scope_from_target_method_and_project(config, alias_to_tasks_map,
                                             alias_to_project_map, aliases_to_scope_map):
    """Determine the restricted scope from both `target_tasks_method` and `project`.

    On certain branches, we'll need differing restricted scopes based on
    `target_tasks_method`.  However, we can't key solely on that, since that
    `target_tasks_method` might be run on an unprivileged branch.  This method
    checks both.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        alias_to_tasks_map (list of lists): each list pair contains the
            alias and the set of target methods that match. This is ordered.
        alias_to_project_map (list of lists): each list pair contains the
            alias and the set of projects that match.  This is ordered.
        aliases_to_scope_map (dict of dicts): the task alias to project alias to scope

    Returns:
        string: the scope to use.
    """
    project = config.params['project']
    target = config.params['target_tasks_method']
    for alias1, tasks in alias_to_tasks_map:
        for alias2, projects in alias_to_project_map:
            if target in tasks and project in projects and \
                    aliases_to_scope_map.get(alias1, {}).get(alias2):
                return aliases_to_scope_map[alias1][alias2]
    return aliases_to_scope_map['default']


def get_phase_from_target_method(config, alias_to_tasks_map, alias_to_phase_map):
    """Determine the phase from `config.params['target_tasks_method']`.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        alias_to_tasks_map (list of lists): each list pair contains the
            alias and the set of target methods that match. This is ordered.
        alias_to_phase_map (dict): the alias to phase map

    Returns:
        string: the phase to use.
    """
    for alias, tasks in alias_to_tasks_map:
        if config.params['target_tasks_method'] in tasks and alias in alias_to_phase_map:
            return alias_to_phase_map[alias]
    return alias_to_phase_map['default']


@with_scope_prefix
def get_balrog_action_scope(config, action='submit'):
    assert action in BALROG_ACTIONS
    return "balrog:action:{}".format(action)


get_signing_cert_scope = functools.partial(
    get_scope_from_project,
    alias_to_project_map=SIGNING_SCOPE_ALIAS_TO_PROJECT,
    alias_to_scope_map=SIGNING_CERT_SCOPES,
)

get_devedition_signing_cert_scope = functools.partial(
    get_scope_from_project,
    alias_to_project_map=DEVEDITION_SIGNING_SCOPE_ALIAS_TO_PROJECT,
    alias_to_scope_map=DEVEDITION_SIGNING_CERT_SCOPES,
)

get_beetmover_bucket_scope = functools.partial(
    get_scope_from_target_method_and_project,
    alias_to_tasks_map=BEETMOVER_SCOPE_ALIAS_TO_TARGET_TASK,
    alias_to_project_map=BEETMOVER_SCOPE_ALIAS_TO_PROJECT,
    aliases_to_scope_map=BEETMOVER_BUCKET_SCOPES,
)

get_beetmover_action_scope = functools.partial(
    get_scope_from_target_method,
    alias_to_tasks_map=BEETMOVER_SCOPE_ALIAS_TO_TARGET_TASK,
    alias_to_scope_map=BEETMOVER_ACTION_SCOPES,
)

get_phase = functools.partial(
    get_phase_from_target_method,
    alias_to_tasks_map=BEETMOVER_SCOPE_ALIAS_TO_TARGET_TASK,
    alias_to_phase_map=PHASES,
)

get_balrog_server_scope = functools.partial(
    get_scope_from_project,
    alias_to_project_map=BALROG_SCOPE_ALIAS_TO_PROJECT,
    alias_to_scope_map=BALROG_SERVER_SCOPES,
)

get_push_apk_scope = functools.partial(
    get_scope_from_project,
    alias_to_project_map=PUSH_APK_SCOPE_ALIAS_TO_PROJECT,
    alias_to_scope_map=PUSH_APK_SCOPES,
)


# release_config {{{1
def get_release_config(config):
    """Get the build number and version for a release task.

    Currently only applies to beetmover tasks.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.

    Returns:
        dict: containing both `build_number` and `version`.  This can be used to
            update `task.payload`.
    """
    release_config = {}

    partial_updates = os.environ.get("PARTIAL_UPDATES", "")
    if partial_updates != "" and config.kind in ('release-bouncer-sub',
                                                 'release-bouncer-check',
                                                 'release-update-verify-config',
                                                 'release-secondary-update-verify-config',
                                                 'release-balrog-submit-toplevel',
                                                 'release-secondary-balrog-submit-toplevel',
                                                 ):
        partial_updates = json.loads(partial_updates)
        release_config['partial_versions'] = ', '.join([
            '{}build{}'.format(v, info['buildNumber'])
            for v, info in partial_updates.items()
        ])
        if release_config['partial_versions'] == "{}":
            del release_config['partial_versions']

    release_config['version'] = str(config.params['version'])
    release_config['appVersion'] = str(config.params['app_version'])

    release_config['next_version'] = str(config.params['next_version'])
    release_config['build_number'] = config.params['build_number']
    return release_config


def get_signing_cert_scope_per_platform(build_platform, is_nightly, config):
    if 'devedition' in build_platform:
        return get_devedition_signing_cert_scope(config)
    elif is_nightly or build_platform in ('firefox-source', 'fennec-source'):
        return get_signing_cert_scope(config)
    else:
        return add_scope_prefix(config, 'signing:cert:dep-signing')


def get_worker_type_for_scope(config, scope):
    """Get the scriptworker type that will accept the given scope.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        scope (string): The scope being used.

    Returns:
        string: The worker-type to use.
    """
    for worker_type, scopes in config.graph_config['scriptworker']['worker-types'].items():
        if scope in scopes:
            return worker_type
    raise RuntimeError(
        "Unsupported scriptworker scope {scope}. (supported scopes: {available_scopes})".format(
            scope=scope,
            available_scopes=sorted(
                scope
                for scopes in config.graph_config['scriptworker']['worker-types'].values()
                for scope in scopes
            ),
        )
    )
