# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph import create
from taskgraph.taskgraph import TaskGraph
from taskgraph.optimize import optimize_task_graph
from taskgraph.util.taskcluster import get_session, find_task_id


def find_decision_task(parameters):
    """Given the parameters for this action, find the taskId of the decision
    task"""
    return find_task_id('gecko.v2.{}.pushlog-id.{}.decision'.format(
        parameters['project'],
        parameters['pushlog_id']))


def create_task_from_def(task_id, task_def, level):
    """Create a new task from a definition rather than from a label
    that is already in the full-task-graph. The task definition will
    have {relative-datestamp': '..'} rendered just like in a decision task.
    Use this for entirely new tasks or ones that change internals of the task.
    It is useful if you want to "edit" the full_task_graph and then hand
    it to this function. No dependencies will be scheduled. You must handle
    this yourself. Seeing how create_tasks handles it might prove helpful."""
    task_def['schedulerId'] = 'gecko-level-{}'.format(level)
    label = task_def['metadata']['name']
    session = get_session()
    create.create_task(session, task_id, label, task_def)


def create_tasks(to_run, full_task_graph, label_to_taskid, params, decision_task_id):
    """Create new tasks.  The task definition will have {relative-datestamp':
    '..'} rendered just like in a decision task.  Action callbacks should use
    this function to create new tasks,
    allowing easy debugging with `mach taskgraph action-callback --test`.
    This builds up all required tasks to run in order to run the tasks requested."""
    to_run = set(to_run)
    target_graph = full_task_graph.graph.transitive_closure(to_run)
    target_task_graph = TaskGraph(
        {l: full_task_graph[l] for l in target_graph.nodes},
        target_graph)
    optimized_task_graph, label_to_taskid = optimize_task_graph(target_task_graph,
                                                                params,
                                                                to_run,
                                                                label_to_taskid)
    create.create_tasks(optimized_task_graph, label_to_taskid, params, decision_task_id)
