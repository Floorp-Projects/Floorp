# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import sys
from abc import ABC, abstractmethod

import attr

from taskgraph.config import GraphConfig
from taskgraph.parameters import Parameters
from taskgraph.taskgraph import TaskGraph
from taskgraph.util.attributes import match_run_on_projects
from taskgraph.util.treeherder import join_symbol

logger = logging.getLogger(__name__)


@attr.s(frozen=True)
class Verification(ABC):
    func = attr.ib()

    @abstractmethod
    def verify(self, **kwargs) -> None:
        pass


@attr.s(frozen=True)
class InitialVerification(Verification):
    """Verification that doesn't depend on any generation state."""

    def verify(self):
        self.func()


@attr.s(frozen=True)
class GraphVerification(Verification):
    """Verification for a TaskGraph object."""

    run_on_projects = attr.ib(default=None)

    def verify(
        self, graph: TaskGraph, graph_config: GraphConfig, parameters: Parameters
    ):
        if self.run_on_projects and not match_run_on_projects(
            parameters["project"], self.run_on_projects
        ):
            return

        scratch_pad = {}
        graph.for_each_task(
            self.func,
            scratch_pad=scratch_pad,
            graph_config=graph_config,
            parameters=parameters,
        )
        self.func(
            None,
            graph,
            scratch_pad=scratch_pad,
            graph_config=graph_config,
            parameters=parameters,
        )


@attr.s(frozen=True)
class ParametersVerification(Verification):
    """Verification for a set of parameters."""

    def verify(self, parameters: Parameters):
        self.func(parameters)


@attr.s(frozen=True)
class KindsVerification(Verification):
    """Verification for kinds."""

    def verify(self, kinds: dict):
        self.func(kinds)


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
    _verification_types = {
        "graph": GraphVerification,
        "initial": InitialVerification,
        "kinds": KindsVerification,
        "parameters": ParametersVerification,
    }

    def __call__(self, name, *args, **kwargs):
        for verification in self._verifications.get(name, []):
            verification.verify(*args, **kwargs)

    def add(self, name, **kwargs):
        cls = self._verification_types.get(name, GraphVerification)

        def wrap(func):
            self._verifications.setdefault(name, []).append(cls(func, **kwargs))
            return func

        return wrap


verifications = VerificationSequence()


@verifications.add("full_task_graph")
def verify_task_graph_symbol(task, taskgraph, scratch_pad, graph_config, parameters):
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
            if len(collection_keys) != 1:
                raise Exception(
                    "Task {} can't be in multiple treeherder collections "
                    "(the part of the platform after `/`): {}".format(
                        task.label, collection_keys
                    )
                )
            platform = treeherder.get("machine", {}).get("platform")
            group_symbol = treeherder.get("groupSymbol")
            symbol = treeherder.get("symbol")

            key = (platform, collection_keys[0], group_symbol, symbol)
            if key in scratch_pad:
                raise Exception(
                    "Duplicate treeherder platform and symbol in tasks "
                    "`{}`and `{}`: {} {}".format(
                        task.label,
                        scratch_pad[key],
                        f"{platform}/{collection_keys[0]}",
                        join_symbol(group_symbol, symbol),
                    )
                )
            else:
                scratch_pad[key] = task.label


@verifications.add("full_task_graph")
def verify_trust_domain_v2_routes(
    task, taskgraph, scratch_pad, graph_config, parameters
):
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
def verify_routes_notification_filters(
    task, taskgraph, scratch_pad, graph_config, parameters
):
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
def verify_dependency_tiers(task, taskgraph, scratch_pad, graph_config, parameters):
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


@verifications.add("full_task_graph")
def verify_toolchain_alias(task, taskgraph, scratch_pad, graph_config, parameters):
    """
    This function verifies that toolchain aliases are not reused.
    """
    if task is None:
        return
    attributes = task.attributes
    if "toolchain-alias" in attributes:
        keys = attributes["toolchain-alias"]
        if not keys:
            keys = []
        elif isinstance(keys, str):
            keys = [keys]
        for key in keys:
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


@verifications.add("optimized_task_graph")
def verify_always_optimized(task, taskgraph, scratch_pad, graph_config, parameters):
    """
    This function ensures that always-optimized tasks have been optimized.
    """
    if task is None:
        return
    if task.task.get("workerType") == "always-optimized":
        raise Exception(f"Could not optimize the task {task.label!r}")
