# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.target_tasks import _target_task, filter_for_tasks_for


@_target_task('default')
def target_tasks_default(full_task_graph, parameters, graph_config):
    """Target the tasks which have indicated they should be run on this project
    via the `run_on_projects` attributes."""

    _filter = filter_for_tasks_for

    # Temporary until we move to shipit for release and beta
    def release(task, parameters):
        return task.attributes.get("release-type", "") == 'release'

    def beta(task, parameters):
        return task.attributes.get("release-type", "") == 'beta'

    if parameters["tasks_for"] == "github-release":
        _filter = release
        if 'beta' in parameters["head_tag"]:
            _filter = beta

    return [l for l, t in full_task_graph.tasks.items() if _filter(t, parameters)]


@_target_task('nightly')
def target_tasks_nightly(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build."""

    def filter(task):
        return task.attributes.get("nightly-task", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t)]