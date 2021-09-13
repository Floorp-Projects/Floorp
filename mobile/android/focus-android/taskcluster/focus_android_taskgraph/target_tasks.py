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


@_target_task("nightly")
def target_tasks_nightly(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build."""
    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


def _filter_fennec(fennec_type, task, parameters):
    return task.attributes.get("build-type", "") == f"fennec-{fennec_type}"


@_target_task("fennec-production")
def target_tasks_fennec_nightly(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a production build signed with the fennec key."""

    return [l for l, t in full_task_graph.tasks.items() if _filter_fennec("production", t, parameters)]


@_target_task("bump_android_components")
def target_tasks_bump_android_components(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to update android components."""

    def filter(task, parameters):
        return task.attributes.get("bump-type", "") == "android-components"

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task("screenshots")
def target_tasks_screnshots(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to generate screenshots on a real device."""

    def filter(task, parameters):
        return task.attributes.get("screenshots", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]
