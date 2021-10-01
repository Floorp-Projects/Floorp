# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.target_tasks import _target_task, filter_for_tasks_for


@_target_task('default')
def target_tasks_default(full_task_graph, parameters, graph_config):
    """Target the tasks which have indicated they should be run on this project
    via the `run_on_projects` attributes."""

    filter = filter_for_tasks_for
    return [l for l, t in full_task_graph.tasks.items() if filter_for_tasks_for(t, parameters)]


@_target_task('release')
def target_tasks_default(full_task_graph, parameters, graph_config):

    # TODO Use shipping-phase once we retire github-releases
    def filter(task, parameters):
        # Mark-as-shipped is always red on github-release and it confuses people.
        # This task cannot be green if we kick off a release through github-releases, so
        # let's exlude that task there.
        if task.kind == "mark-as-shipped" and parameters["tasks_for"] == "github-release":
            return False

        return task.attributes.get("release-type", "") == parameters["release_type"]

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task('nightly')
def target_tasks_nightly(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build."""

    def filter(task):
        return task.attributes.get("nightly-task", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t)]