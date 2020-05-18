# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import re
import os

import attr
import six

from .. import GECKO
from .treeherder import join_symbol

logger = logging.getLogger(__name__)
doc_base_path = os.path.join(GECKO, 'taskcluster', 'docs')


@attr.s(frozen=True)
class VerificationSequence(object):
    """
    Container for a sequence of verifications over a TaskGraph. Each
    verification is represented as a callable taking (task, taskgraph,
    scratch_pad), called for each task in the taskgraph, and one more
    time with no task but with the taskgraph and the same scratch_pad
    that was passed for each task.
    """
    _verifications = attr.ib(factory=dict)

    def __call__(self, graph_name, graph, graph_config):
        for verification in self._verifications.get(graph_name, []):
            scratch_pad = {}
            graph.for_each_task(verification, scratch_pad=scratch_pad, graph_config=graph_config)
            verification(None, graph, scratch_pad=scratch_pad, graph_config=graph_config)
        return graph_name, graph

    def add(self, graph_name):
        def wrap(func):
            self._verifications.setdefault(graph_name, []).append(func)
            return func
        return wrap


verifications = VerificationSequence()


@attr.s(frozen=True)
class DocPaths(object):
    _paths = attr.ib(factory=list)

    def get_files(self, filename):
        rv = []
        for p in self._paths:
            doc_path = os.path.join(p, filename)
            if os.path.exists(doc_path):
                rv.append(doc_path)
        return rv

    def add(self, path):
        """
        Projects that make use of Firefox's taskgraph can extend it with
        their own task kinds by registering additional paths for documentation.
        documentation_paths.add() needs to be called by the project's Taskgraph
        registration function. See taskgraph.config.
        """
        self._paths.append(path)


documentation_paths = DocPaths()
documentation_paths.add(doc_base_path)


def verify_docs(filename, identifiers, appearing_as):
    """
    Look for identifiers of the type appearing_as in the files
    returned by documentation_paths.get_files(). Firefox will have
    a single file in a list, but projects such as Thunderbird can have
    documentation in another location and may return multiple files.
    """
    # We ignore identifiers starting with '_' for the sake of tests.
    # Strings starting with "_" are ignored for doc verification
    # hence they can be used for faking test values
    doc_files = documentation_paths.get_files(filename)
    doctext = "".join(
        [open(d).read() for d in doc_files]
    )

    if appearing_as == "inline-literal":
        expression_list = [
            "``" + identifier + "``"
            for identifier in identifiers
            if not identifier.startswith("_")
        ]
    elif appearing_as == "heading":
        expression_list = [
            '\n' + identifier + "\n(?:(?:(?:-+\n)+)|(?:(?:.+\n)+))"
            for identifier in identifiers
            if not identifier.startswith("_")
        ]
    else:
        raise Exception("appearing_as = `{}` not defined".format(appearing_as))

    for expression, identifier in zip(expression_list, identifiers):
        match_group = re.search(expression, doctext)
        if not match_group:
            raise Exception(
                "{}: `{}` missing from doc file: `{}`"
                .format(appearing_as, identifier, filename)
            )


@verifications.add('full_task_graph')
def verify_task_graph_symbol(task, taskgraph, scratch_pad, graph_config):
    """
        This function verifies that tuple
        (collection.keys(), machine.platform, groupSymbol, symbol) is unique
        for a target task graph.
    """
    if task is None:
        return
    task_dict = task.task
    if "extra" in task_dict:
        extra = task_dict["extra"]
        if "treeherder" in extra:
            treeherder = extra["treeherder"]

            collection_keys = tuple(sorted(treeherder.get('collection', {}).keys()))
            if len(collection_keys) != 1:
                raise Exception(
                    "Task {} can't be in multiple treeherder collections "
                    "(the part of the platform after `/`): {}"
                    .format(task.label, collection_keys)
                )
            platform = treeherder.get('machine', {}).get('platform')
            group_symbol = treeherder.get('groupSymbol')
            symbol = treeherder.get('symbol')

            key = (platform, collection_keys[0], group_symbol, symbol)
            if key in scratch_pad:
                raise Exception(
                    "Duplicate treeherder platform and symbol in tasks "
                    "`{}`and `{}`: {} {}".format(
                        task.label,
                        scratch_pad[key],
                        "{}/{}".format(platform, collection_keys[0]),
                        join_symbol(group_symbol, symbol),
                    )
                )
            else:
                scratch_pad[key] = task.label


@verifications.add('full_task_graph')
def verify_trust_domain_v2_routes(task, taskgraph, scratch_pad, graph_config):
    """
    This function ensures that any two tasks have distinct ``index.{trust-domain}.v2`` routes.
    """
    if task is None:
        return
    route_prefix = "index.{}.v2".format(graph_config['trust-domain'])
    task_dict = task.task
    routes = task_dict.get('routes', [])

    for route in routes:
        if route.startswith(route_prefix):
            if route in scratch_pad:
                raise Exception(
                    "conflict between {}:{} for route: {}"
                    .format(task.label, scratch_pad[route], route)
                )
            else:
                scratch_pad[route] = task.label


@verifications.add('full_task_graph')
def verify_routes_notification_filters(task, taskgraph, scratch_pad, graph_config):
    """
        This function ensures that only understood filters for notifications are
        specified.

        See: https://firefox-ci-tc.services.mozilla.com/docs/manual/using/task-notifications
    """
    if task is None:
        return
    route_prefix = "notify."
    valid_filters = ('on-any', 'on-completed', 'on-failed', 'on-exception')
    task_dict = task.task
    routes = task_dict.get('routes', [])

    for route in routes:
        if route.startswith(route_prefix):
            # Get the filter of the route
            route_filter = route.split('.')[-1]
            if route_filter not in valid_filters:
                raise Exception(
                    '{} has invalid notification filter ({})'
                    .format(task.label, route_filter)
                )


@verifications.add('full_task_graph')
def verify_dependency_tiers(task, taskgraph, scratch_pad, graph_config):
    tiers = scratch_pad
    if task is not None:
        tiers[task.label] = task.task.get('extra', {}) \
                                     .get('treeherder', {}) \
                                     .get('tier', six.MAXSIZE)
    else:
        def printable_tier(tier):
            if tier == six.MAXSIZE:
                return 'unknown'
            return tier

        for task in six.itervalues(taskgraph.tasks):
            tier = tiers[task.label]
            for d in six.itervalues(task.dependencies):
                if taskgraph[d].task.get("workerType") == "always-optimized":
                    continue
                if "dummy" in taskgraph[d].kind:
                    continue
                if tier < tiers[d]:
                    raise Exception(
                        '{} (tier {}) cannot depend on {} (tier {})'
                        .format(task.label, printable_tier(tier),
                                d, printable_tier(tiers[d])))


@verifications.add('full_task_graph')
def verify_required_signoffs(task, taskgraph, scratch_pad, graph_config):
    """
    Task with required signoffs can't be dependencies of tasks with less
    required signoffs.
    """
    all_required_signoffs = scratch_pad
    if task is not None:
        all_required_signoffs[task.label] = set(task.attributes.get('required_signoffs', []))
    else:
        def printable_signoff(signoffs):
            if len(signoffs) == 1:
                return 'required signoff {}'.format(*signoffs)
            elif signoffs:
                return 'required signoffs {}'.format(', '.join(signoffs))
            else:
                return 'no required signoffs'
        for task in six.itervalues(taskgraph.tasks):
            required_signoffs = all_required_signoffs[task.label]
            for d in six.itervalues(task.dependencies):
                if required_signoffs < all_required_signoffs[d]:
                    raise Exception(
                        '{} ({}) cannot depend on {} ({})'
                        .format(task.label, printable_signoff(required_signoffs),
                                d, printable_signoff(all_required_signoffs[d])))


@verifications.add('full_task_graph')
def verify_toolchain_alias(task, taskgraph, scratch_pad, graph_config):
    """
        This function verifies that toolchain aliases are not reused.
    """
    if task is None:
        return
    attributes = task.attributes
    if "toolchain-alias" in attributes:
        key = attributes['toolchain-alias']
        if key in scratch_pad:
            raise Exception(
                "Duplicate toolchain-alias in tasks "
                "`{}`and `{}`: {}".format(
                    task.label,
                    scratch_pad[key],
                    key,
                )
            )
        else:
            scratch_pad[key] = task.label


@verifications.add('optimized_task_graph')
def verify_always_optimized(task, taskgraph, scratch_pad, graph_config):
    """
        This function ensures that always-optimized tasks have been optimized.
    """
    if task is None:
        return
    if task.task.get('workerType') == 'always-optimized':
        raise Exception('Could not optimize the task {!r}'.format(task.label))


@verifications.add('full_task_graph')
def verify_nightly_no_sccache(task, taskgraph, scratch_pad, graph_config):
    if task and any([task.attributes.get('nightly'), task.attributes.get('shippable')]):
        if task.task.get('payload', {}).get('env', {}).get('USE_SCCACHE'):
            raise Exception(
                'Nightly job {} cannot use sccache'.format(task.label))


@verifications.add('full_task_graph')
def verify_test_packaging(task, taskgraph, scratch_pad, graph_config):
    if task is None:
        exceptions = []
        for task in six.itervalues(taskgraph.tasks):
            if task.kind == 'build' and not task.attributes.get('skip-verify-test-packaging'):
                build_env = task.task.get('payload', {}).get('env', {})
                package_tests = build_env.get('MOZ_AUTOMATION_PACKAGE_TESTS')
                shippable = task.attributes.get('shippable', False)
                nightly = task.attributes.get('nightly', False)
                build_has_tests = scratch_pad.get(task.label)

                if package_tests != '1':
                    # Shippable builds should always package tests.
                    if shippable:
                        exceptions.append('Build job {} is shippable and does not specify '
                                          'MOZ_AUTOMATION_PACKAGE_TESTS=1 in the '
                                          'environment.'.format(task.label))
                    if nightly:
                        exceptions.append('Build job {} is nightly and does not specify '
                                          'MOZ_AUTOMATION_PACKAGE_TESTS=1 in the '
                                          'environment.'.format(task.label))

                    # Build tasks in the scratch pad have tests dependent on
                    # them, so we need to package tests during build.
                    if build_has_tests:
                        exceptions.append(
                            'Build job {} has tests dependent on it and does not specify '
                            'MOZ_AUTOMATION_PACKAGE_TESTS=1 in the environment'.format(task.label))
                else:
                    # Build tasks that aren't in the scratch pad have no
                    # dependent tests, so we shouldn't package tests.
                    # With the caveat that we expect shippable and nightly jobs to always
                    # produce tests.
                    if not build_has_tests and not any([shippable, nightly]):
                        exceptions.append(
                            'Build job {} has no tests, but specifies '
                            'MOZ_AUTOMATION_PACKAGE_TESTS={} in the environment. '
                            'Unset MOZ_AUTOMATION_PACKAGE_TESTS in the task definition '
                            'to fix.'.format(task.label, package_tests))
        if exceptions:
            raise Exception("\n".join(exceptions))
        return
    if task.kind == 'test':
        build_task = taskgraph[task.dependencies['build']]
        scratch_pad[build_task.label] = 1
