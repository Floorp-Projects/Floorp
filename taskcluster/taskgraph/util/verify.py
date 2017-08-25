# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import os

base_path = os.path.join(os.getcwd(), "taskcluster/docs/")


class VerificationSequence(object):
    """
    Container for a sequence of verifications over a TaskGraph. Each
    verification is represented as a callable taking (task, taskgraph,
    scratch_pad), called for each task in the taskgraph.
    """
    def __init__(self):
        self.verifications = {}

    def __call__(self, graph_name, graph):
        for verification in self.verifications.get(graph_name, []):
            graph.for_each_task(verification, scratch_pad={})
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
