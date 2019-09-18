# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.target_tasks import _target_task, standard_filter


# XXX We're overwritting the default target_task while all tasks are ported to taskgraph
@_target_task('default')
def target_tasks_default(full_task_graph, parameters, graph_config):
    """Target the tasks which have indicated they should be run on this project
    via the `run_on_projects` attributes."""
    return [l for l, t in full_task_graph.tasks.iteritems() if _old_decision_filter(t, parameters)]


def _old_decision_filter(task, parameters):
    if task.kind == 'old-decision':
        return True

    return standard_filter(task, parameters)


@_target_task("nightly")
def target_tasks_nightly(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build."""

    def filter(task, parameters):
        if task.attributes.get("nightly", False):
            return True

        return _old_decision_filter(task, parameters)

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('raptor')
def target_tasks_raptor(full_task_graph, parameters, graph_config):
    def filter(t, params):
        if t.kind == 'raptor':
            return True

        return _old_decision_filter(t, params)

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]
