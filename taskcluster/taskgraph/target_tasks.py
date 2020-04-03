# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph import try_option_syntax
from taskgraph.parameters import Parameters
from taskgraph.util.attributes import match_run_on_projects, match_run_on_hg_branches

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
    return (
        not task.attributes.get('nightly') and
        task.attributes.get('shipping_phase') in (None, 'build')
        )


def filter_out_cron(task, parameters):
    """
    Filter out tasks that run via cron.
    """
    return not task.attributes.get('cron')


def filter_for_project(task, parameters):
    """Filter tasks by project.  Optionally enable nightlies."""
    run_on_projects = set(task.attributes.get('run_on_projects', []))
    return match_run_on_projects(parameters['project'], run_on_projects)


def filter_for_hg_branch(task, parameters):
    """Filter tasks by hg branch.
    If `run_on_hg_branch` is not defined, then task runs on all branches"""
    run_on_hg_branches = set(task.attributes.get('run_on_hg_branches', ['all']))
    return match_run_on_hg_branches(parameters['hg_branch'], run_on_hg_branches)


def filter_on_platforms(task, platforms):
    """Filter tasks on the given platform"""
    platform = task.attributes.get('build_platform')
    return (platform in platforms)


def filter_release_tasks(task, parameters):
    platform = task.attributes.get('build_platform')
    if platform in (
            # On beta, Nightly builds are already PGOs
            'linux-pgo', 'linux64-pgo',
            'win32-pgo', 'win64-pgo',
            ):
        return False

    if platform in (
            'linux', 'linux64',
            'macosx64',
            'win32', 'win64', 'win64-aarch64',
            ):
        if task.attributes['kind'] == 'l10n':
            # This is on-change l10n
            return True
        if task.attributes['build_type'] == 'opt' and \
           task.attributes.get('unittest_suite') != 'talos' and \
           task.attributes.get('unittest_suite') != 'raptor':
            return False

    if task.attributes.get('shipping_phase') not in (None, 'build'):
        return False

    return True


def filter_out_missing_signoffs(task, parameters):
    for signoff in parameters['required_signoffs']:
        if signoff not in parameters['signoff_urls'] and \
           signoff in task.attributes.get('required_signoffs', []):
            return False
    return True


def standard_filter(task, parameters):
    return all(
        filter_func(task, parameters) for filter_func in
        (filter_out_cron, filter_for_project, filter_for_hg_branch)
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

        # If the developer wants test talos jobs to be rebuilt N times we add that value here
        if options.talos_trigger_tests > 1 and task.attributes.get('unittest_suite') == 'talos':
            task.attributes['task_duplicates'] = options.talos_trigger_tests

        # If the developer wants test raptor jobs to be rebuilt N times we add that value here
        if options.raptor_trigger_tests > 1 and task.attributes.get('unittest_suite') == 'raptor':
            task.attributes['task_duplicates'] = options.raptor_trigger_tests

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


@_target_task('try_auto')
def target_tasks_try_auto(full_task_graph, parameters, graph_config):
    """Target the tasks which have indicated they should be run on autoland
    (rather than try) via the `run_on_projects` attributes.

    Should do the same thing as the `default` target tasks method.
    """
    params = dict(parameters)
    params['project'] = 'autoland'
    parameters = Parameters(**params)
    return [l for l, t in full_task_graph.tasks.iteritems()
            if standard_filter(t, parameters)
            and filter_out_nightly(t, parameters)]


@_target_task('default')
def target_tasks_default(full_task_graph, parameters, graph_config):
    """Target the tasks which have indicated they should be run on this project
    via the `run_on_projects` attributes."""
    return [l for l, t in full_task_graph.tasks.iteritems()
            if standard_filter(t, parameters)
            and filter_out_nightly(t, parameters)]


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
    """Target tasks for mochitest valgrind jobs."""
    def filter(task):
        platform = task.attributes.get('test_platform', '').split('/')[0]
        if platform not in ['linux64']:
            return False

        if task.attributes.get('unittest_suite', '').startswith('mochitest-valgrind-plain'):
            return True
        return False

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('mozilla_beta_tasks')
def target_tasks_mozilla_beta(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a promotable beta or release build
    of desktop, plus android CI. The candidates build process involves a pipeline
    of builds and signing, but does not include beetmover or balrog jobs."""

    return [l for l, t in full_task_graph.tasks.iteritems()
            if filter_release_tasks(t, parameters)
            and standard_filter(t, parameters)]


@_target_task('mozilla_release_tasks')
def target_tasks_mozilla_release(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a promotable beta or release build
    of desktop, plus android CI. The candidates build process involves a pipeline
    of builds and signing, but does not include beetmover or balrog jobs."""

    return [l for l, t in full_task_graph.tasks.iteritems()
            if filter_release_tasks(t, parameters)
            and standard_filter(t, parameters)]


@_target_task('mozilla_esr68_tasks')
def target_tasks_mozilla_esr68(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a promotable beta or release build
    of desktop, plus android CI. The candidates build process involves a pipeline
    of builds and signing, but does not include beetmover or balrog jobs."""

    def filter(task):
        if not filter_release_tasks(task, parameters):
            return False

        if not standard_filter(task, parameters):
            return False

        platform = task.attributes.get('test_platform')

        # Don't run QuantumRender tests on esr68.
        if platform and '-qr/' in platform:
            return False

        # Unlike esr60, we do want all kinds of fennec builds on esr68.

        return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('promote_desktop')
def target_tasks_promote_desktop(full_task_graph, parameters, graph_config):
    """Select the superset of tasks required to promote a beta or release build
    of a desktop product. This should include all non-android
    mozilla_{beta,release} tasks, plus l10n, beetmover, balrog, etc."""

    def filter(task):
        if task.attributes.get('shipping_product') != parameters['release_product']:
            return False

        # 'secondary' balrog/update verify/final verify tasks only run for RCs
        if parameters.get('release_type') != 'release-rc':
            if 'secondary' in task.kind:
                return False

        if not filter_out_missing_signoffs(task, parameters):
            return False

        if task.attributes.get('shipping_phase') == 'promote':
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


def is_geckoview(task, parameters):
    return (
        task.attributes.get('shipping_product') == 'fennec' and
        task.kind in ('beetmover-geckoview', 'upload-symbols') and
        parameters['release_product'] == 'firefox'
    )


@_target_task('push_desktop')
def target_tasks_push_desktop(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to push a build of desktop to cdns.
    Previous build deps will be optimized out via action task."""
    filtered_for_candidates = target_tasks_promote_desktop(
        full_task_graph, parameters, graph_config,
    )

    def filter(task):
        if not filter_out_missing_signoffs(task, parameters):
            return False
        # Include promotion tasks; these will be optimized out
        if task.label in filtered_for_candidates:
            return True
        # XXX: Bug 1612540 - include beetmover jobs for publishing geckoview, along
        # with the regular Firefox (not Devedition!) releases so that they are at sync
        if is_geckoview(task, parameters):
            return True

        if task.attributes.get('shipping_product') == parameters['release_product'] and \
                task.attributes.get('shipping_phase') == 'push':
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('ship_desktop')
def target_tasks_ship_desktop(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to ship desktop.
    Previous build deps will be optimized out via action task."""
    is_rc = (parameters.get('release_type') == 'release-rc')
    if is_rc:
        # ship_firefox_rc runs after `promote` rather than `push`; include
        # all promote tasks.
        filtered_for_candidates = target_tasks_promote_desktop(
            full_task_graph, parameters, graph_config,
        )
    else:
        # ship_firefox runs after `push`; include all push tasks.
        filtered_for_candidates = target_tasks_push_desktop(
            full_task_graph, parameters, graph_config,
        )

    def filter(task):
        if not filter_out_missing_signoffs(task, parameters):
            return False
        # Include promotion tasks; these will be optimized out
        if task.label in filtered_for_candidates:
            return True

        # XXX: Bug 1619603 - geckoview also ships alongside Firefox RC
        if is_geckoview(task, parameters) and is_rc:
            return True

        if task.attributes.get('shipping_product') != parameters['release_product'] or \
                task.attributes.get('shipping_phase') != 'ship':
            return False

        if 'secondary' in task.kind:
            return is_rc
        else:
            return not is_rc

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('promote_fennec')
def target_tasks_promote_fennec(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a candidates build of fennec. The
    nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""

    def filter(task):
        attr = task.attributes.get
        # Don't ship single locale fennec anymore - Bug 1408083
        if attr("locale") or attr("chunk_locales"):
            return False
        if task.attributes.get('shipping_product') == 'fennec' and \
                task.attributes.get('shipping_phase') == 'promote':
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(full_task_graph[l])]


@_target_task('ship_fennec')
def target_tasks_ship_fennec(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to ship fennec.
    Previous build deps will be optimized out via action task."""
    is_rc = (parameters.get('release_type') == 'release-rc')
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
        if standard_filter(task, parameters) or filter_out_nightly(task, parameters):
            return True
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('ship_geckoview')
def target_tasks_ship_geckoview(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to ship geckoview nightly. The
    nightly build process involves a pipeline of builds and an upload to
    maven.mozilla.org."""
    def filter(task):
        # XXX Starting 69, we don't ship Fennec Nightly anymore. We just want geckoview to be
        # uploaded
        return (
            task.attributes.get('shipping_product') == 'fennec' and
            task.kind in ('beetmover-geckoview', 'upload-symbols')
        )

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('fennec_v68')
def target_tasks_fennec_v68(full_task_graph, parameters, graph_config):
    """
    Select tasks required for running tp6m fennec v68 tests
    """
    def filter(task):
        platform = task.attributes.get('build_platform')
        test_platform = task.attributes.get('test_platform')
        attributes = task.attributes

        if platform and 'android' not in platform:
            return False
        if attributes.get('unittest_suite') != 'raptor':
            return False
        if '-fennec68-' in attributes.get('raptor_try_name'):
            if '-p2' in test_platform and '-arm7' in test_platform:
                return False
            if '-youtube-playback-' in attributes.get('raptor_try_name') \
                    and 'opt' in test_platform:
                return False
            return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('general_perf_testing')
def target_tasks_general_perf_testing(full_task_graph, parameters, graph_config):
    """
    Select tasks required for running performance tests 3 times a week.
    """
    def filter(task):
        platform = task.attributes.get('build_platform')
        attributes = task.attributes
        if attributes.get('unittest_suite') != 'raptor':
            return False
        try_name = attributes.get('raptor_try_name')

        # Run chrome and chromium on all platforms available
        if '-chrome' in try_name:
            return True
        if '-chromium' in try_name:
            return True

        # Run raptor scn-power-idle and speedometer for fenix and fennec68
        if 'raptor-scn-power-idle' in try_name \
                and 'pgo' in platform \
                and ('-fenix' in try_name or '-fennec68' in try_name):
            return True
        if 'raptor-speedometer' in try_name \
                and 'pgo' in platform \
                and ('-fenix' in try_name or '-fennec68' in try_name):
            return True

        # Run the following tests on android geckoview
        if platform and 'android' not in platform:
            return False
        if 'geckoview' not in try_name:
            return False

        # Run cpu+memory, and power tests
        cpu_n_memory_task = '-cpu' in try_name and '-memory' in try_name
        power_task = '-power' in try_name
        # Ignore cpu+memory+power tests
        if power_task and cpu_n_memory_task:
            return False
        if power_task or cpu_n_memory_task:
            if 'pgo' not in platform:
                return False
            if '-speedometer-' in try_name:
                return True
            if '-scn' in try_name and '-idle' in try_name:
                return True
        return False

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


def make_desktop_nightly_filter(platforms):
    """Returns a filter that gets all nightly tasks on the given platform."""
    def filter(task, parameters):
        return all([
            filter_on_platforms(task, platforms),
            filter_for_project(task, parameters),
            any([
                task.attributes.get('nightly', False),
                task.attributes.get('shippable', False),
            ]),
            # Tests and nightly only builds don't have `shipping_product` set
            task.attributes.get('shipping_product') in {None, "firefox", "thunderbird"},
            task.kind not in {'l10n'},  # no on-change l10n
        ])
    return filter


@_target_task('nightly_linux')
def target_tasks_nightly_linux(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of linux. The
    nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_desktop_nightly_filter({
        'linux64-nightly', 'linux-nightly', 'linux64-shippable', 'linux-shippable'
        })
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('nightly_macosx')
def target_tasks_nightly_macosx(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of macosx. The
    nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_desktop_nightly_filter({'macosx64-nightly', 'macosx64-shippable'})
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('nightly_win32')
def target_tasks_nightly_win32(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of win32 and win64.
    The nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_desktop_nightly_filter({'win32-nightly', 'win32-shippable'})
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('nightly_win64')
def target_tasks_nightly_win64(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of win32 and win64.
    The nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_desktop_nightly_filter({'win64-nightly', 'win64-shippable'})
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('nightly_win64_aarch64')
def target_tasks_nightly_win64_aarch64(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of win32 and win64.
    The nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_desktop_nightly_filter({'win64-aarch64-nightly', 'win64-aarch64-shippable'})
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('nightly_asan')
def target_tasks_nightly_asan(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of asan. The
    nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    filter = make_desktop_nightly_filter({
        'linux64-asan-reporter-nightly',
        'win64-asan-reporter-nightly'
    })
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('daily_releases')
def target_tasks_daily_releases(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to identify if we should release.
    If we determine that we should the task will communicate to ship-it to
    schedule the release itself."""

    def filter(task):
        return task.kind in ['maybe-release']

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('nightly_desktop')
def target_tasks_nightly_desktop(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build of linux, mac,
    windows."""
    # Tasks that aren't platform specific
    release_filter = make_desktop_nightly_filter({None})
    release_tasks = [
        l for l, t in full_task_graph.tasks.iteritems()
        if release_filter(t, parameters)
    ]
    # Avoid duplicate tasks.
    return list(
        set(target_tasks_nightly_win32(full_task_graph, parameters, graph_config))
        | set(target_tasks_nightly_win64(full_task_graph, parameters, graph_config))
        | set(target_tasks_nightly_win64_aarch64(full_task_graph, parameters, graph_config))
        | set(target_tasks_nightly_macosx(full_task_graph, parameters, graph_config))
        | set(target_tasks_nightly_linux(full_task_graph, parameters, graph_config))
        | set(target_tasks_nightly_asan(full_task_graph, parameters, graph_config))
        | set(release_tasks)
    )


# Run Searchfox analysis once daily.
@_target_task('searchfox_index')
def target_tasks_searchfox(full_task_graph, parameters, graph_config):
    """Select tasks required for indexing Firefox for Searchfox web site each day"""
    return ['searchfox-linux64-searchfox/debug',
            'searchfox-macosx64-searchfox/debug',
            'searchfox-win64-searchfox/debug',
            'searchfox-android-armv7-searchfox/debug',
            'source-test-file-metadata-bugzilla-components']


@_target_task('customv8_update')
def target_tasks_customv8_update(full_task_graph, parameters, graph_config):
    """Select tasks required for building latest d8/v8 version."""
    return ['toolchain-linux64-custom-v8']


@_target_task('chromium_update')
def target_tasks_chromium_update(full_task_graph, parameters, graph_config):
    """Select tasks required for building latest chromium versions."""
    return ['fetch-linux64-chromium',
            'fetch-win32-chromium',
            'fetch-win64-chromium',
            'fetch-mac-chromium']


@_target_task('python_dependency_update')
def target_tasks_python_update(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to perform nightly in-tree pipfile updates
    """
    def filter(task):
        # For now any task in the repo-update kind is ok
        return task.kind in ['python-dependency-update']
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('file_update')
def target_tasks_file_update(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to perform nightly in-tree file updates
    """
    def filter(task):
        # For now any task in the repo-update kind is ok
        return task.kind in ['repo-update']
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('l10n_bump')
def target_tasks_l10n_bump(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to perform l10n bumping.
    """
    def filter(task):
        # For now any task in the repo-update kind is ok
        return task.kind in ['l10n-bump']
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('merge_automation')
def target_tasks_merge_automation(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to perform repository merges.
    """
    def filter(task):
        # For now any task in the repo-update kind is ok
        return task.kind in ['merge-automation']
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('cron_bouncer_check')
def target_tasks_bouncer_check(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to perform bouncer version verification.
    """
    def filter(task):
        if not filter_for_project(task, parameters):
            return False
        # For now any task in the repo-update kind is ok
        return task.kind in ['cron-bouncer-check']
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('staging_release_builds')
def target_tasks_staging_release(full_task_graph, parameters, graph_config):
    """
    Select all builds that are part of releases.
    """

    def filter(task):
        if not task.attributes.get('shipping_product'):
            return False
        if (parameters['release_type'].startswith('esr')
                and 'android' in task.attributes.get('build_platform', '')):
            return False
        if (parameters['release_type'] != 'beta'
                and 'devedition' in task.attributes.get('build_platform', '')):
            return False
        if task.attributes.get('shipping_phase') == 'build':
            return True
        return False

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('release_simulation')
def target_tasks_release_simulation(full_task_graph, parameters, graph_config):
    """
    Select builds that would run on push on a release branch.
    """
    project_by_release = {
        'nightly': 'mozilla-central',
        'beta': 'mozilla-beta',
        'release': 'mozilla-release',
        'esr68': 'mozilla-esr68',
    }
    target_project = project_by_release.get(parameters['release_type'])
    if target_project is None:
        raise Exception('Unknown or unspecified release type in simulation run.')

    def filter_for_target_project(task):
        """Filter tasks by project.  Optionally enable nightlies."""
        run_on_projects = set(task.attributes.get('run_on_projects', []))
        return match_run_on_projects(target_project, run_on_projects)

    def filter_out_android_on_esr(task):
        if (parameters['release_type'].startswith('esr')
                and 'android' in task.attributes.get('build_platform', '')):
            return False
        return True

    return [l for l, t in full_task_graph.tasks.iteritems()
            if filter_release_tasks(t, parameters)
            and filter_out_cron(t, parameters)
            and filter_for_target_project(t)
            and filter_out_android_on_esr(t)]


@_target_task('codereview')
def target_tasks_codereview(full_task_graph, parameters, graph_config):
    """Select all code review tasks needed to produce a report"""

    def filter(task):
        # Ending tasks
        if task.kind in ['code-review']:
            return True

        # Analyzer tasks
        if task.attributes.get('code-review') is True:
            return True

        return False

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('nothing')
def target_tasks_nothing(full_task_graph, parameters, graph_config):
    """Select nothing, for DONTBUILD pushes"""
    return []


@_target_task('raptor_tp6m')
def target_tasks_raptor_tp6m(full_task_graph, parameters, graph_config):
    """
    Select tasks required for running raptor cold page-load tests on fenix and refbrow
    """
    def filter(task):
        platform = task.attributes.get('build_platform')
        attributes = task.attributes

        if platform and 'android' not in platform:
            return False
        if attributes.get('unittest_suite') != 'raptor':
            return False
        try_name = attributes.get('raptor_try_name')
        if '-cold' in try_name and 'pgo' in platform:
            if '-1-refbrow-' in try_name:
                return True
            if '-1-fenix-' in try_name:
                return True

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('condprof')
def target_tasks_condprof(full_task_graph, parameters, graph_config):
    """
    Select tasks required for building conditioned profiles.
    """
    for name, task in full_task_graph.tasks.iteritems():
        if task.kind == "condprof":
            yield name


@_target_task('system_symbols')
def target_tasks_system_symbols(full_task_graph, parameters, graph_config):
    """
    Select tasks for uploading system-symbols.
    """
    for name, task in full_task_graph.tasks.iteritems():
        if task.kind == "system-symbols-upload":
            yield name
