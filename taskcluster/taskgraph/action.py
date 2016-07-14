# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import logging
import requests

from .create import create_tasks
from .decision import write_artifact
from .parameters import Parameters
from .optimize import optimize_task_graph
from .taskgraph import TaskGraph

logger = logging.getLogger(__name__)
TASKCLUSTER_QUEUE_URL = "https://queue.taskcluster.net/v1/task/"


def taskgraph_action(options):
    """
    Run the action task.  This function implements `mach taskgraph action-task`,
    and is responsible for

     * creating taskgraph of tasks asked for in parameters with respect to
     a given gecko decision task and schedule these jobs.
    """

    parameters = get_action_parameters(options)
    decision_task_id = parameters['decision_id']
    # read in the full graph for reference
    full_task_json = get_artifact(decision_task_id, "public/full-task-graph.json")
    all_tasks, full_task_graph = TaskGraph.from_json(full_task_json, options['root'])

    target_tasks = set(parameters['task_labels'].split(','))
    target_graph = full_task_graph.graph.transitive_closure(target_tasks)
    target_task_graph = TaskGraph(
        {l: all_tasks[l] for l in target_graph.nodes},
        target_graph)

    existing_tasks = get_artifact(decision_task_id, "public/label-to-taskid.json")

    # We don't want to optimize target tasks since they have been requested by user
    # Hence we put `target_tasks under` `do_not_optimize`
    optimized_graph, label_to_taskid = optimize_task_graph(target_task_graph=target_task_graph,
                                                           do_not_optimize=target_tasks,
                                                           existing_tasks=existing_tasks)

    # write out the optimized task graph to describe what will actually happen,
    # and the map of labels to taskids
    write_artifact('task-graph.json', optimized_graph.to_json())
    write_artifact('label-to-taskid.json', label_to_taskid)
    # actually create the graph
    create_tasks(optimized_graph, label_to_taskid)


def get_action_parameters(options):
    """
    Load parameters from the command-line options for 'taskgraph action'.
    """
    parameters = {n: options[n] for n in [
        'decision_id',
        'task_labels',
    ] if n in options}

    return Parameters(parameters)


def get_artifact(task_id, path):
    url = TASKCLUSTER_QUEUE_URL + task_id + "/artifacts/" + path
    resp = requests.get(url=url)
    artifact = json.loads(resp.text)
    return artifact
