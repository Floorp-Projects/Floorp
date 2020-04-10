# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Tools for interacting with existing taskgraphs.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.taskcluster import (
    find_task_id,
    get_artifact,
)


def find_decision_task(parameters, graph_config):
    """Given the parameters for this action, find the taskId of the decision
    task"""
    head_rev_param = '{}head_rev'.format(graph_config['project-repo-param-prefix'])
    return find_task_id('{}.v2.{}.revision.{}.taskgraph.decision'.format(
        graph_config['trust-domain'],
        parameters['project'],
        parameters[head_rev_param]))


def find_existing_tasks(previous_graph_ids):
    existing_tasks = {}
    for previous_graph_id in previous_graph_ids:
        label_to_taskid = get_artifact(previous_graph_id, "public/label-to-taskid.json")
        existing_tasks.update(label_to_taskid)
    return existing_tasks


def find_existing_tasks_from_previous_kinds(
    full_task_graph, previous_graph_ids, rebuild_kinds
):
    """Given a list of previous decision/action taskIds and kinds to ignore
    from the previous graphs, return a dictionary of labels-to-taskids to use
    as ``existing_tasks`` in the optimization step."""
    existing_tasks = find_existing_tasks(previous_graph_ids)
    kind_labels = {
        t.label
        for t in full_task_graph.tasks.itervalues()
        if t.attributes["kind"] not in rebuild_kinds
    }
    return {
        label: taskid
        for (label, taskid) in existing_tasks.items()
        if label in kind_labels
    }
