# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import re
import os
import sys

from .. import GECKO
from taskgraph.util.bbb_validation import valid_bbb_builders

logger = logging.getLogger(__name__)
base_path = os.path.join(GECKO, 'taskcluster', 'docs')


class VerificationSequence(object):
    """
    Container for a sequence of verifications over a TaskGraph. Each
    verification is represented as a callable taking (task, taskgraph,
    scratch_pad), called for each task in the taskgraph, and one more
    time with no task but with the taskgraph and the same scratch_pad
    that was passed for each task.
    """
    def __init__(self):
        self.verifications = {}

    def __call__(self, graph_name, graph):
        for verification in self.verifications.get(graph_name, []):
            scratch_pad = {}
            graph.for_each_task(verification, scratch_pad=scratch_pad)
            verification(None, graph, scratch_pad=scratch_pad)
        return graph_name, graph

    def add(self, graph_name):
        def wrap(func):
            self.verifications.setdefault(graph_name, []).append(func)
            return func
        return wrap


verifications = VerificationSequence()


def verify_docs(filename, identifiers, appearing_as):

    # We ignore identifiers starting with '_' for the sake of tests.
    # Strings starting with "_" are ignored for doc verification
    # hence they can be used for faking test values
    with open(os.path.join(base_path, filename)) as fileObject:
        doctext = "".join(fileObject.readlines())
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
def verify_task_graph_symbol(task, taskgraph, scratch_pad):
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
            platform = treeherder.get('machine', {}).get('platform')
            group_symbol = treeherder.get('groupSymbol')
            symbol = treeherder.get('symbol')

            key = (collection_keys, platform, group_symbol, symbol)
            if key in scratch_pad:
                raise Exception(
                    "conflict between `{}`:`{}` for values `{}`"
                    .format(task.label, scratch_pad[key], key)
                )
            else:
                scratch_pad[key] = task.label


@verifications.add('full_task_graph')
def verify_gecko_v2_routes(task, taskgraph, scratch_pad):
    """
        This function ensures that any two
        tasks have distinct index.v2.routes
    """
    if task is None:
        return
    route_prefix = "index.gecko.v2"
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
def verify_dependency_tiers(task, taskgraph, scratch_pad):
    tiers = scratch_pad
    if task is not None:
        tiers[task.label] = task.task.get('extra', {}) \
                                     .get('treeherder', {}) \
                                     .get('tier', sys.maxint)
    else:
        def printable_tier(tier):
            if tier == sys.maxint:
                return 'unknown'
            return tier

        for task in taskgraph.tasks.itervalues():
            # Buildbot bridge tasks cannot have tiers, so we cannot enforce
            # this check for them
            if task.task.get("workerType") == "buildbot-bridge":
                continue
            tier = tiers[task.label]
            for d in task.dependencies.itervalues():
                if taskgraph[d].task.get("workerType") in ("buildbot-bridge",
                                                           "always-optimized"):
                    continue
                if "dummy" in taskgraph[d].kind:
                    continue
                if tier < tiers[d]:
                    raise Exception(
                        '{} (tier {}) cannot depend on {} (tier {})'
                        .format(task.label, printable_tier(tier),
                                d, printable_tier(tiers[d])))


@verifications.add('optimized_task_graph')
def verify_bbb_builders_valid(task, taskgraph, scratch_pad):
    """
        This function ensures that any task which is run
        in buildbot (via buildbot-bridge) is using a recognized buildername.

        If you see an unexpected failure with a task due to this check, please
        see the IRC Channel, #releng.
    """
    if task is None:
        return
    valid_builders = valid_bbb_builders()
    if valid_builders is None:
        return
    if task.task.get('workerType') == 'buildbot-bridge':
        buildername = task.task['payload']['buildername']
        if buildername not in valid_builders:
            logger.warning(
                '{} uses an invalid buildbot buildername ("{}") '
                ' - contact #releng for help'
                .format(task.label, buildername))


@verifications.add('optimized_task_graph')
def verify_always_optimized(task, taskgraph, scratch_pad):
    """
        This function ensures that always-optimized tasks have been optimized.
    """
    if task is None:
        return
    if task.task.get('workerType') == 'always-optimized':
        raise Exception('Could not optimize the task {!r}'.format(task.label))
