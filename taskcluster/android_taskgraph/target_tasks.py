# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from redo import retry
from taskgraph.target_tasks import _target_task
from taskgraph.util.taskcluster import find_task_id

from android_taskgraph.release_type import does_task_match_release_type


def index_exists(index_path, reason=""):
    print(f"Looking for existing index {index_path} {reason}...")
    try:
        task_id = find_task_id(index_path)
        print(f"Index {index_path} exists: taskId {task_id}")
        return True
    except KeyError:
        print(f"Index {index_path} doesn't exist.")
        return False


@_target_task("nightly")
def target_tasks_nightly(full_task_graph, parameters, graph_config):
    def filter(task, parameters):
        build_type = task.attributes.get("build-type", "")
        return build_type in (
            "nightly",
            "focus-nightly",
            "fenix-nightly",
            "fenix-nightly-firebase",
            "focus-nightly-firebase",
        )

    index_path = (
        f"{graph_config['trust-domain']}.v2.{parameters['project']}.branch."
        f"{parameters['head_ref']}.revision.{parameters['head_rev']}.taskgraph.decision-nightly"
    )
    if os.environ.get("MOZ_AUTOMATION") and retry(
        index_exists,
        args=(index_path,),
        kwargs={
            "reason": "to avoid triggering multiple nightlies off the same revision",
        },
    ):
        return []

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task("nightly-test")
def target_tasks_nightly_test(full_task_graph, parameters, graph_config):
    def filter(task, parameters):
        return task.attributes.get("nightly-test", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task("promote_android")
def target_tasks_promote(full_task_graph, parameters, graph_config):
    return _filter_release_promotion(
        full_task_graph,
        parameters,
        filtered_for_candidates=[],
        shipping_phase="promote",
    )


@_target_task("push_android")
def target_tasks_push(full_task_graph, parameters, graph_config):
    filtered_for_candidates = target_tasks_promote(
        full_task_graph,
        parameters,
        graph_config,
    )
    return _filter_release_promotion(
        full_task_graph, parameters, filtered_for_candidates, shipping_phase="push"
    )


@_target_task("ship_android")
def target_tasks_ship(full_task_graph, parameters, graph_config):
    filtered_for_candidates = target_tasks_push(
        full_task_graph,
        parameters,
        graph_config,
    )
    return _filter_release_promotion(
        full_task_graph, parameters, filtered_for_candidates, shipping_phase="ship"
    )


def _filter_release_promotion(
    full_task_graph, parameters, filtered_for_candidates, shipping_phase
):
    def filter(task, parameters):
        # Include promotion tasks; these will be optimized out
        if task.label in filtered_for_candidates:
            return True

        # TODO: get rid of this release_type match
        if (
            task.attributes.get("shipping_phase") == shipping_phase
            and task.attributes.get("shipping_product") == parameters["release_product"]
            and does_task_match_release_type(task, parameters["release_type"])
        ):
            return True

        return False

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task("screenshots")
def target_tasks_screnshots(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to generate screenshots on a real device."""

    def filter(task, parameters):
        return task.attributes.get("screenshots", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task("legacy_api_ui_tests")
def target_tasks_legacy_api_ui_tests(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to run select UI tests on other API."""

    def filter(task, parameters):
        return task.attributes.get("legacy", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]
