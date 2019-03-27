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
    return find_task_id('{}.v2.{}.pushlog-id.{}.decision'.format(
        graph_config['trust-domain'],
        parameters['project'],
        parameters['pushlog_id']))


def find_existing_tasks_from_previous_kinds(full_task_graph, previous_graph_ids,
                                            rebuild_kinds):
    """Given a list of previous decision/action taskIds and kinds to ignore
    from the previous graphs, return a dictionary of labels-to-taskids to use
    as ``existing_tasks`` in the optimization step."""
    existing_tasks = {}
    for previous_graph_id in previous_graph_ids:
        label_to_taskid = get_artifact(previous_graph_id, "public/label-to-taskid.json")
        kind_labels = set(t.label for t in full_task_graph.tasks.itervalues()
                          if t.attributes['kind'] not in rebuild_kinds)
        for label in set(label_to_taskid.keys()).intersection(kind_labels):
            existing_tasks[label] = label_to_taskid[label]
    return existing_tasks
