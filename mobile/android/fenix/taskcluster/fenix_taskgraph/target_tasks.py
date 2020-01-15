# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.target_tasks import _target_task, filter_for_tasks_for


@_target_task('default')
def target_tasks_default(full_task_graph, parameters, graph_config):
    """Target the tasks which have indicated they should be run on this project
    via the `run_on_projects` attributes."""

    filter = filter_for_tasks_for
    return [l for l, t in full_task_graph.tasks.iteritems() if filter_for_tasks_for(t, parameters)]


@_target_task('release')
def target_tasks_default(full_task_graph, parameters, graph_config):

    def filter(task, parameters):
        return task.attributes.get("release-type", "") == parameters["release_type"]

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task("nightly")
def target_tasks_nightly(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build."""

    def filter(task, parameters):
        return (
            task.attributes.get("nightly", False) and
            not _filter_fennec_nightly(task, parameters)
        )

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]

def _filter_fennec_nightly(task, parameters):
    return task.attributes.get("build-type", "") == "fennec-nightly"

@_target_task("fennec-nightly")
def target_tasks_fennec_nightly(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build signed with the fennec key."""

    return [l for l, t in full_task_graph.tasks.iteritems() if _filter_fennec_nightly(t, parameters)]


@_target_task('raptor')
def target_tasks_raptor(full_task_graph, parameters, graph_config):
    def filter(task, parameters):
        return task.kind == 'raptor'

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]
