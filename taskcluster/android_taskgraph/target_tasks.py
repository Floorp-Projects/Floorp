# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.target_tasks import register_target_task

from android_taskgraph.release_type import does_task_match_release_type


@register_target_task("promote_android")
def target_tasks_promote(full_task_graph, parameters, graph_config):
    return _filter_release_promotion(
        full_task_graph,
        parameters,
        filtered_for_candidates=[],
        shipping_phase="promote",
    )


@register_target_task("push_android")
def target_tasks_push(full_task_graph, parameters, graph_config):
    filtered_for_candidates = target_tasks_promote(
        full_task_graph,
        parameters,
        graph_config,
    )
    return _filter_release_promotion(
        full_task_graph, parameters, filtered_for_candidates, shipping_phase="push"
    )


@register_target_task("ship_android")
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

        # Ship geckoview in firefox-android ship graph
        if (
            shipping_phase == "ship"
            and task.attributes.get("shipping_product") == "fennec"
            and task.kind in ("beetmover-geckoview", "upload-symbols")
            and parameters["release_product"] == "firefox-android"
        ):
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


@register_target_task("screenshots")
def target_tasks_screnshots(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to generate screenshots on a real device."""

    def filter(task, parameters):
        return task.attributes.get("screenshots", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@register_target_task("legacy_api_ui_tests")
def target_tasks_legacy_api_ui_tests(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to run select UI tests on other API."""

    def filter(task, parameters):
        return task.attributes.get("legacy", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]
