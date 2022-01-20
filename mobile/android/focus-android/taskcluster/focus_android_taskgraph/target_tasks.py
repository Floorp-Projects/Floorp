# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.target_tasks import _target_task


@_target_task('release')
def target_tasks_default(full_task_graph, parameters, graph_config):
    """Target the tasks (release or beta) triggered by a shipit phase action."""

    def filter(task, parameters):
        return task.attributes.get("release-type", "") == parameters["release_type"]

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task('nightly')
def target_tasks_nightly(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build."""

    def filter(task):
        return task.attributes.get("nightly-task", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t)]


