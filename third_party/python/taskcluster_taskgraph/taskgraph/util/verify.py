# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import sys

import attr

logger = logging.getLogger(__name__)


@attr.s(frozen=True)
class VerificationSequence:
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
            graph.for_each_task(
                verification, scratch_pad=scratch_pad, graph_config=graph_config
            )
            verification(
                None, graph, scratch_pad=scratch_pad, graph_config=graph_config
            )
        return graph_name, graph

    def add(self, graph_name):
        def wrap(func):
            self._verifications.setdefault(graph_name, []).append(func)
            return func

        return wrap


verifications = VerificationSequence()


@verifications.add("full_task_graph")
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

            collection_keys = tuple(sorted(treeherder.get("collection", {}).keys()))
            platform = treeherder.get("machine", {}).get("platform")
            group_symbol = treeherder.get("groupSymbol")
            symbol = treeherder.get("symbol")

            key = (collection_keys, platform, group_symbol, symbol)
            if key in scratch_pad:
                raise Exception(
                    "conflict between `{}`:`{}` for values `{}`".format(
                        task.label, scratch_pad[key], key
                    )
                )
            else:
                scratch_pad[key] = task.label


@verifications.add("full_task_graph")
def verify_trust_domain_v2_routes(task, taskgraph, scratch_pad, graph_config):
    """
    This function ensures that any two tasks have distinct ``index.{trust-domain}.v2`` routes.
    """
    if task is None:
        return
    route_prefix = "index.{}.v2".format(graph_config["trust-domain"])
    task_dict = task.task
    routes = task_dict.get("routes", [])

    for route in routes:
        if route.startswith(route_prefix):
            if route in scratch_pad:
                raise Exception(
                    "conflict between {}:{} for route: {}".format(
                        task.label, scratch_pad[route], route
                    )
                )
            else:
                scratch_pad[route] = task.label


@verifications.add("full_task_graph")
def verify_routes_notification_filters(task, taskgraph, scratch_pad, graph_config):
    """
    This function ensures that only understood filters for notifications are
    specified.

    See: https://docs.taskcluster.net/reference/core/taskcluster-notify/docs/usage
    """
    if task is None:
        return
    route_prefix = "notify."
    valid_filters = ("on-any", "on-completed", "on-failed", "on-exception")
    task_dict = task.task
    routes = task_dict.get("routes", [])

    for route in routes:
        if route.startswith(route_prefix):
            # Get the filter of the route
            route_filter = route.split(".")[-1]
            if route_filter not in valid_filters:
                raise Exception(
                    "{} has invalid notification filter ({})".format(
                        task.label, route_filter
                    )
                )


@verifications.add("full_task_graph")
def verify_dependency_tiers(task, taskgraph, scratch_pad, graph_config):
    tiers = scratch_pad
    if task is not None:
        tiers[task.label] = (
            task.task.get("extra", {}).get("treeherder", {}).get("tier", sys.maxsize)
        )
    else:

        def printable_tier(tier):
            if tier == sys.maxsize:
                return "unknown"
            return tier

        for task in taskgraph.tasks.values():
            tier = tiers[task.label]
            for d in task.dependencies.values():
                if taskgraph[d].task.get("workerType") == "always-optimized":
                    continue
                if "dummy" in taskgraph[d].kind:
                    continue
                if tier < tiers[d]:
                    raise Exception(
                        "{} (tier {}) cannot depend on {} (tier {})".format(
                            task.label,
                            printable_tier(tier),
                            d,
                            printable_tier(tiers[d]),
                        )
                    )


@verifications.add("optimized_task_graph")
def verify_always_optimized(task, taskgraph, scratch_pad, graph_config):
    """
    This function ensures that always-optimized tasks have been optimized.
    """
    if task is None:
        return
    if task.task.get("workerType") == "always-optimized":
        raise Exception(f"Could not optimize the task {task.label!r}")
