# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph import try_option_syntax
from taskgraph.util.attributes import match_run_on_projects

_target_task_methods = {}


def _target_task(name):
    def wrap(func):
        _target_task_methods[name] = func
        return func
    return wrap


def get_method(method):
    """Get a target_task_method to pass to a TaskGraphGenerator."""
    return _target_task_methods[method]


def filter_out_nightly(task, parameters):
    return not task.attributes.get('nightly') or parameters.get('include_nightly')


def filter_for_project(task, parameters):
    """Filter tasks by project.  Optionally enable nightlies."""
    run_on_projects = set(task.attributes.get('run_on_projects', []))
    return match_run_on_projects(parameters['project'], run_on_projects)


def filter_on_platforms(task, platforms):
    """Filter tasks on the given platform"""
    platform = task.attributes.get('build_platform')
    return (platform in platforms)


def filter_upload_symbols(task, parameters):
    # Filters out symbols when there are not part of a nightly or a release build
    # TODO Remove this too specific filter (bug 1353296)
    return '-upload-symbols' not in task.label or \
        task.attributes.get('nightly') or \
        parameters.get('project') in ('mozilla-beta', 'mozilla-release')


def filter_beta_release_tasks(task, parameters, ignore_kinds=None, allow_l10n=False):
    if not standard_filter(task, parameters):
        return False
    if ignore_kinds is None:
        ignore_kinds = [
            'balrog',
            'beetmover', 'beetmover-checksums', 'beetmover-l10n',
            'beetmover-repackage', 'beetmover-repackage-signing',
            'checksums-signing',
            'nightly-l10n', 'nightly-l10n-signing',
            'push-apk', 'repackage-l10n',
        ]
    platform = task.attributes.get('build_platform')
    if platform in (
            # On beta, Nightly builds are already PGOs
            'linux-pgo', 'linux64-pgo',
            'win32-pgo', 'win64-pgo',
            ):
        return False
    if str(platform).startswith('android') and 'nightly' in str(platform):
        return False

    if platform in (
            'linux', 'linux64',
            'macosx64',
            'win32', 'win64',
            ):
        if task.attributes['build_type'] == 'opt' and \
           task.attributes.get('unittest_suite') != 'talos':
            return False

    # skip l10n, beetmover, balrog
    if task.kind in ignore_kinds:
        return False

    # No l10n repacks per push. They may be triggered by kinds which depend
    # on l10n builds/repacks. For instance: "repackage-signing"
    if not allow_l10n and task.attributes.get('locale', '') != '':
        return False

    return True


def standard_filter(task, parameters):
    return all(
        filter_func(task, parameters) for filter_func in
        (filter_out_nightly, filter_for_project, filter_upload_symbols)
    )


def _try_task_config(full_task_graph, parameters, graph_config):
    requested_tasks = parameters['try_task_config']['tasks']
    return list(set(requested_tasks) & full_task_graph.graph.nodes)


def _try_option_syntax(full_task_graph, parameters, graph_config):
    """Generate a list of target tasks based on try syntax in
    parameters['message'] and, for context, the full task graph."""
    options = try_option_syntax.TryOptionSyntax(parameters, full_task_graph, graph_config)
    target_tasks_labels = [t.label for t in full_task_graph.tasks.itervalues()
                           if options.task_matches(t)]

    attributes = {
        k: getattr(options, k) for k in [
            'env',
            'no_retry',
            'tag',
        ]
    }

    for l in target_tasks_labels:
        task = full_task_graph[l]
        if 'unittest_suite' in task.attributes:
            task.attributes['task_duplicates'] = options.trigger_tests

    for l in target_tasks_labels:
        task = full_task_graph[l]
        # If the developer wants test jobs to be rebuilt N times we add that value here
        if options.trigger_tests > 1 and 'unittest_suite' in task.attributes:
            task.attributes['task_duplicates'] = options.trigger_tests
            task.attributes['profile'] = False

        # If the developer wants test talos jobs to be rebuilt N times we add that value here
        if options.talos_trigger_tests > 1 and task.attributes.get('unittest_suite') == 'talos':
            task.attributes['task_duplicates'] = options.talos_trigger_tests
            task.attributes['profile'] = options.profile

        task.attributes.update(attributes)

    # Add notifications here as well
    if options.notifications:
        for task in full_task_graph:
            owner = parameters.get('owner')
            routes = task.task.setdefault('routes', [])
            if options.notifications == 'all':
                routes.append("notify.email.{}.on-any".format(owner))
            elif options.notifications == 'failure':
                routes.append("notify.email.{}.on-failed".format(owner))
                routes.append("notify.email.{}.on-exception".format(owner))

    return target_tasks_labels


@_target_task('try_tasks')
def target_tasks_try(full_task_graph, parameters, graph_config):
    try_mode = parameters['try_mode']
    if try_mode == 'try_task_config':
        return _try_task_config(full_task_graph, parameters, graph_config)
    elif try_mode == 'try_option_syntax':
        return _try_option_syntax(full_task_graph, parameters, graph_config)
    else:
        # With no try mode, we schedule nothing, allowing the user to add tasks
        # later via treeherder.
        return []


@_target_task('default')
def target_tasks_default(full_task_graph, parameters, graph_config):
    """Target the tasks which have indicated they should be run on this project
    via the `run_on_projects` attributes."""
    return [l for l, t in full_task_graph.tasks.iteritems()
            if standard_filter(t, parameters)]


@_target_task('ash_tasks')
def target_tasks_ash(full_task_graph, parameters, graph_config):
    """Target tasks that only run on the ash branch."""
    def filter(task):
        platform = task.attributes.get('build_platform')
        # Early return if platform is None
        if not platform:
            return False
        # Only on Linux platforms
        if 'linux' not in platform:
            return False
        # No random non-build jobs either. This is being purposely done as a
        # blacklist so newly-added jobs aren't missed by default.
        for p in ('nightly', 'haz', 'artifact', 'cov', 'add-on'):
            if p in platform:
                return False
        for k in ('toolchain', 'l10n', 'static-analysis'):
            if k in task.attributes['kind']:
                return False
        # and none of this linux64-asan/debug stuff
        if platform == 'linux64-asan' and task.attributes['build_type'] == 'debug':
            return False
        # no non-e10s tests
        if task.attributes.get('unittest_suite'):
            if not task.attributes.get('e10s'):
                return False
            # don't run talos on ash
            if task.attributes.get('unittest_suite') == 'talos':
                return False
        # don't upload symbols
        if task.attributes['kind'] == 'upload-symbols':
            return False
        return True
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('cedar_tasks')
def target_tasks_cedar(full_task_graph, parameters, graph_config):
    """Target tasks that only run on the cedar branch."""
    def filter(task):
        platform = task.attributes.get('build_platform')
        # only select platforms
        if platform not in ('linux64', 'macosx64'):
            return False
        if task.attributes.get('unittest_suite'):
            if not (task.attributes['unittest_suite'].startswith('mochitest') or
                    'xpcshell' in task.attributes['unittest_suite']):
                return False
        return True
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('graphics_tasks')
def target_tasks_graphics(full_task_graph, parameters, graph_config):
    """In addition to doing the filtering by project that the 'default'
       filter does, also remove artifact builds because we have csets on
       the graphics branch that aren't on the candidate branches of artifact
       builds"""
    filtered_for_project = target_tasks_default(full_task_graph, parameters, graph_config)

    def filter(task):
        if task.attributes['kind'] == 'artifact-build':
            return False
        return True
    return [l for l in filtered_for_project if filter(full_task_graph[l])]


@_target_task('mochitest_valgrind')
def target_tasks_valgrind(full_task_graph, parameters, graph_config):
    """Target tasks that only run on the cedar branch."""
    def filter(task):
        platform = task.attributes.get('test_platform', '').split('/')[0]
        if platform not in ['linux64']:
            return False

        if task.attributes.get('unittest_suite', '').startswith('mochitest') and \
           task.attributes.get('unittest_flavor', '').startswith('valgrind-plain'):
            return True
        return False

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('mozilla_beta_tasks')
def target_tasks_mozilla_beta(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a promotable beta or release build
    of desktop, plus android CI. The candidates build process involves a pipeline
    of builds and signing, but does not include beetmover or balrog jobs."""

    return [l for l, t in full_task_graph.tasks.iteritems() if
            filter_beta_release_tasks(t, parameters)]


@_target_task('mozilla_release_tasks')
def target_tasks_mozilla_release(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a promotable beta or release build
    of desktop, plus android CI. The candidates build process involves a pipeline
    of builds and signing, but does not include beetmover or balrog jobs."""

    return [l for l, t in full_task_graph.tasks.iteritems() if
            filter_beta_release_tasks(t, parameters)]


@_target_task('promote_firefox')
def target_tasks_promote_firefox(full_task_graph, parameters, graph_config):
    """Select the superset of tasks required to promote a beta or release build
    of firefox. This should include all non-android mozilla_{beta,release} tasks,
    plus l10n, beetmover, balrog, etc."""

    beta_release_tasks = [l for l, t in full_task_graph.tasks.iteritems() if
                          filter_beta_release_tasks(t, parameters,
                                                    ignore_kinds=[],
                                                    allow_l10n=True)]

    def filter(task):
        platform = task.attributes.get('build_platform')

        # Android has its own promotion.
        if platform and 'android' in platform:
            return False

        if platform and 'devedition' in platform:
            return False

        # Allow for {beta,release}_tasks; these will get optimized out to point
        # to the previous graph using ``previous_graph_ids`` and
        # ``previous_graph_kinds``.
        # At some point this should filter by shipping_phase == 'build' and
        # shipping_product matches.
        if task.label in beta_release_tasks:
            return True

        # 'secondary' balrog/update verify/final verify tasks only run for RCs
        if parameters.get('release_type') != 'rc':
            if 'secondary' in task.kind:
                return False

        if task.attributes.get('shipping_product') == 'firefox' and \
                task.attributes.get('shipping_phase') == 'promote':
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('push_firefox')
def target_tasks_push_firefox(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to push a build of firefox to cdns.
    Previous build deps will be optimized out via action task."""
    filtered_for_candidates = target_tasks_promote_firefox(
        full_task_graph, parameters, graph_config,
    )

    def filter(task):
        # Include promotion tasks; these will be optimized out
        if task.label in filtered_for_candidates:
            return True
        if task.attributes.get('shipping_product') == 'firefox' and \
                task.attributes.get('shipping_phase') == 'push':
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('ship_firefox')
def target_tasks_ship_firefox(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to ship firefox.
    Previous build deps will be optimized out via action task."""
    is_rc = (parameters.get('release_type') == 'rc')
    if is_rc:
        # ship_firefox_rc runs after `promote` rather than `push`; include
        # all promote tasks.
        filtered_for_candidates = target_tasks_promote_firefox(
            full_task_graph, parameters, graph_config,
        )
    else:
        # ship_firefox runs after `push`; include all push tasks.
        filtered_for_candidates = target_tasks_push_firefox(
            full_task_graph, parameters, graph_config,
        )

    def filter(task):
        # Include promotion tasks; these will be optimized out
        if task.label in filtered_for_candidates:
            return True
        if task.attributes.get('shipping_product') != 'firefox' or \
                task.attributes.get('shipping_phase') != 'ship':
            return False

        if 'secondary' in task.kind:
                return is_rc
        else:
                return not is_rc

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('promote_devedition')
def target_tasks_promote_devedition(full_task_graph, parameters, graph_config):
    """Select the superset of tasks required to promote a beta or release build
    of devedition. This should include all non-android mozilla_{beta,release}
    tasks, plus l10n, beetmover, balrog, etc."""

    beta_release_tasks = [l for l, t in full_task_graph.tasks.iteritems() if
                          filter_beta_release_tasks(t, parameters,
                                                    ignore_kinds=[],
                                                    allow_l10n=True)]

    def filter(task):
        platform = task.attributes.get('build_platform')

        # Android has its own promotion.
        if platform and 'android' in platform:
            return False

        if platform and 'devedition' not in platform:
            return False

        # Allow for {beta,release}_tasks; these will get optimized out to point to
        # the previous graph using ``previous_graph_ids`` and
        # ``previous_graph_kinds``.
        # At some point this should filter by shipping_phase == 'build' and
        # shipping_product matches.
        if task.label in beta_release_tasks:
            return True

        if task.attributes.get('shipping_product') == 'devedition' and \
                task.attributes.get('shipping_phase') == 'promote':
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('push_devedition')
def target_tasks_push_devedition(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to push a build of devedition to cdns.
    Previous build deps will be optimized out via action task."""
    filtered_for_candidates = target_tasks_promote_devedition(
        full_task_graph, parameters, graph_config,
    )

    def filter(task):
        # Include promotion tasks; these will be optimized out
        if task.label in filtered_for_candidates:
            return True
        if task.attributes.get('shipping_product') == 'devedition' and \
                task.attributes.get('shipping_phase') == 'push':
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('ship_devedition')
def target_tasks_ship_devedition(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to ship devedition.
    Previous build deps will be optimized out via action task."""
    filtered_for_candidates = target_tasks_push_devedition(
        full_task_graph, parameters, graph_config,
    )

    def filter(task):
        # Include promotion tasks; these will be optimized out
        if task.label in filtered_for_candidates:
            return True
        if task.attributes.get('shipping_product') == 'devedition' and \
                task.attributes.get('shipping_phase') == 'ship':
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('promote_fennec')
def target_tasks_promote_fennec(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a candidates build of fennec. The
    nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filtered_for_project = target_tasks_nightly_fennec(full_task_graph, parameters, graph_config)

    def filter(task):
        attr = task.attributes.get
        # Don't ship single locale fennec anymore - Bug 1408083
        if attr("locale") or attr("chunk_locales"):
            return False
        if task.label in filtered_for_project:
            if task.kind not in ('balrog', 'push-apk'):
                if task.attributes.get('nightly'):
                    return True
        if task.attributes.get('shipping_product') == 'fennec' and \
                task.attributes.get('shipping_phase') == 'promote':
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(full_task_graph[l])]


@_target_task('ship_fennec')
def target_tasks_ship_fennec(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to ship fennec.
    Previous build deps will be optimized out via action task."""
    is_rc = (parameters.get('release_type') == 'rc')
    filtered_for_candidates = target_tasks_promote_fennec(
        full_task_graph, parameters, graph_config,
    )

    def filter(task):
        # Include candidates build tasks; these will be optimized out
        if task.label in filtered_for_candidates:
            return True
        if task.attributes.get('shipping_product') != 'fennec' or \
                task.attributes.get('shipping_phase') not in ('ship', 'push'):
            return False
        # We always run push-apk during ship
        if task.kind == 'push-apk':
            return True
        # secondary-notify-ship is only for RC
        if task.kind in (
            'release-secondary-notify-ship',
        ):
            return is_rc

        # Everything else is only for non-RC
        return not is_rc

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(full_task_graph[l])]


@_target_task('pine_tasks')
def target_tasks_pine(full_task_graph, parameters, graph_config):
    """Bug 1339179 - no mobile automation needed on pine"""
    def filter(task):
        platform = task.attributes.get('build_platform')
        # disable mobile jobs
        if str(platform).startswith('android'):
            return False
        # disable asan
        if platform == 'linux64-asan':
            return False
        # disable non-pine and nightly tasks
        if standard_filter(task, parameters):
            return True
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('nightly_fennec')
def target_tasks_nightly_fennec(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of fennec. The
    nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    def filter(task):
        platform = task.attributes.get('build_platform')
        if not filter_for_project(task, parameters):
            return False
        if platform in ('android-aarch64-nightly',
                        'android-api-16-nightly',
                        'android-nightly',
                        'android-x86-nightly',
                        ):
            if not task.attributes.get('nightly', False):
                return False
            return filter_for_project(task, parameters)
    filter
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


def make_nightly_filter(platforms):
    """Returns a filter that gets all nightly tasks on the given platform."""
    def filter(task, parameters):
        return all([
            filter_on_platforms(task, platforms),
            filter_for_project(task, parameters),
            task.attributes.get('nightly', False),
        ])
    return filter


@_target_task('nightly_linux')
def target_tasks_nightly_linux(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of linux. The
    nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_nightly_filter({'linux64-nightly', 'linux-nightly'})
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('nightly_macosx')
def target_tasks_nightly_macosx(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of macosx. The
    nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_nightly_filter({'macosx64-nightly'})
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('nightly_win32')
def target_tasks_nightly_win32(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of win32 and win64.
    The nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_nightly_filter({'win32-nightly'})
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('nightly_win64')
def target_tasks_nightly_win64(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of win32 and win64.
    The nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_nightly_filter({'win64-nightly'})
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('nightly_desktop')
def target_tasks_nightly_desktop(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of linux, mac,
    windows."""
    # Avoid duplicate tasks.
    return list(
        set(target_tasks_nightly_win32(full_task_graph, parameters, graph_config))
        | set(target_tasks_nightly_win64(full_task_graph, parameters, graph_config))
        | set(target_tasks_nightly_macosx(full_task_graph, parameters, graph_config))
        | set(target_tasks_nightly_linux(full_task_graph, parameters, graph_config))
    )


# Opt DMD builds should only run nightly
@_target_task('nightly_dmd')
def target_tasks_dmd(full_task_graph, parameters, graph_config):
    """Target DMD that run nightly on the m-c branch."""
    def filter(task):
        platform = task.attributes.get('build_platform', '')
        return platform.endswith('-dmd')
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


# Run Searchfox analysis once daily.
@_target_task('searchfox_index')
def target_tasks_searchfox(full_task_graph, parameters, graph_config):
    """Select tasks required for indexing Firefox for Searchfox web site each day"""
    # For now we only do Linux debug builds. Windows and Mac builds
    # are currently broken (bug 1418415).
    return ['searchfox-linux64-searchfox/debug']


@_target_task('file_update')
def target_tasks_file_update(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to perform nightly in-tree file updates
    """
    def filter(task):
        # For now any task in the repo-update kind is ok
        return task.kind in ['repo-update']
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]
