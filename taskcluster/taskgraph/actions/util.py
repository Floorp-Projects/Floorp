# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import requests
import os

from requests.exceptions import HTTPError

from taskgraph import create
from taskgraph.decision import write_artifact
from taskgraph.taskgraph import TaskGraph
from taskgraph.optimize import optimize_task_graph
from taskgraph.util.taskcluster import get_session, find_task_id, get_artifact, list_tasks

logger = logging.getLogger(__name__)

PUSHLOG_TMPL = '{}/json-pushes?version=2&changeset={}&tipsonly=1&full=1'


def find_decision_task(parameters):
    """Given the parameters for this action, find the taskId of the decision
    task"""
    return find_task_id('gecko.v2.{}.pushlog-id.{}.decision'.format(
        parameters['project'],
        parameters['pushlog_id']))


def find_hg_revision_pushlog_id(parameters, revision):
    """Given the parameters for this action and a revision, find the
    pushlog_id of the revision."""
    pushlog_url = PUSHLOG_TMPL.format(parameters['head_repository'], revision)
    r = requests.get(pushlog_url)
    r.raise_for_status()
    pushes = r.json()['pushes'].keys()
    if len(pushes) != 1:
        raise RuntimeError(
            "Unable to find a single pushlog_id for {} revision {}: {}".format(
                parameters['head_repository'], revision, pushes
            )
        )
    return pushes[0]


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


def fetch_graph_and_labels(parameters):
    decision_task_id = find_decision_task(parameters)

    # First grab the graph and labels generated during the initial decision task
    full_task_graph = get_artifact(decision_task_id, "public/full-task-graph.json")
    _, full_task_graph = TaskGraph.from_json(full_task_graph)
    label_to_taskid = get_artifact(decision_task_id, "public/label-to-taskid.json")

    # Now fetch any modifications made by action tasks and swap out new tasks
    # for old ones
    namespace = 'gecko.v2.{}.pushlog-id.{}.actions'.format(
        parameters['project'],
        parameters['pushlog_id'])
    for action in list_tasks(namespace):
        try:
            run_label_to_id = get_artifact(action, "public/label-to-taskid.json")
            label_to_taskid.update(run_label_to_id)
        except HTTPError as e:
            logger.info('Skipping {} due to missing artifact! Error: {}'.format(action, e))
            continue

    return (decision_task_id, full_task_graph, label_to_taskid)


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


def update_parent(task, graph):
    task.task.setdefault('extra', {})['parent'] = os.environ.get('TASK_ID', '')
    return task


def create_tasks(to_run, full_task_graph, label_to_taskid, params, decision_task_id=None):
    """Create new tasks.  The task definition will have {relative-datestamp':
    '..'} rendered just like in a decision task.  Action callbacks should use
    this function to create new tasks,
    allowing easy debugging with `mach taskgraph action-callback --test`.
    This builds up all required tasks to run in order to run the tasks requested.

    If you wish to create the tasks in a new group, leave out decision_task_id."""
    to_run = set(to_run)
    target_graph = full_task_graph.graph.transitive_closure(to_run)
    target_task_graph = TaskGraph(
        {l: full_task_graph[l] for l in target_graph.nodes},
        target_graph)
    target_task_graph.for_each_task(update_parent)
    optimized_task_graph, label_to_taskid = optimize_task_graph(target_task_graph,
                                                                params,
                                                                to_run,
                                                                label_to_taskid)
    write_artifact('task-graph.json', optimized_task_graph.to_json())
    write_artifact('label-to-taskid.json', label_to_taskid)
    write_artifact('to-run.json', list(to_run))
    create.create_tasks(optimized_task_graph, label_to_taskid, params, decision_task_id)
