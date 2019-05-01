# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import concurrent.futures as futures
import copy
import logging
import os
import re

from six import text_type

from requests.exceptions import HTTPError

from taskgraph import create
from taskgraph.decision import read_artifact, write_artifact, rename_artifact
from taskgraph.taskgraph import TaskGraph
from taskgraph.optimize import optimize_task_graph
from taskgraph.util.taskcluster import (
    get_session,
    get_artifact,
    list_tasks,
    parse_time,
    CONCURRENCY,
)
from taskgraph.util.taskgraph import (
    find_decision_task,
)

logger = logging.getLogger(__name__)


def fetch_graph_and_labels(parameters, graph_config):
    decision_task_id = find_decision_task(parameters, graph_config)

    # First grab the graph and labels generated during the initial decision task
    full_task_graph = get_artifact(decision_task_id, "public/full-task-graph.json")
    _, full_task_graph = TaskGraph.from_json(full_task_graph)
    label_to_taskid = get_artifact(decision_task_id, "public/label-to-taskid.json")

    # fetch everything in parallel; this avoids serializing any delay in downloading
    # each artifact (such as waiting for the artifact to be mirrored locally)
    with futures.ThreadPoolExecutor(CONCURRENCY) as e:
        fetches = []

        # fetch any modifications made by action tasks and swap out new tasks
        # for old ones
        def fetch_action(task_id):
            logger.info('fetching label-to-taskid.json for action task {}'.format(task_id))
            try:
                run_label_to_id = get_artifact(task_id, "public/label-to-taskid.json")
                label_to_taskid.update(run_label_to_id)
            except HTTPError as e:
                if e.response.status_code != 404:
                    raise
                logger.debug('No label-to-taskid.json found for {}: {}'.format(task_id, e))

        namespace = '{}.v2.{}.pushlog-id.{}.actions'.format(
            graph_config['trust-domain'],
            parameters['project'],
            parameters['pushlog_id'])
        for task_id in list_tasks(namespace):
            fetches.append(e.submit(fetch_action, task_id))

        # Similarly for cron tasks..
        def fetch_cron(task_id):
            logger.info('fetching label-to-taskid.json for cron task {}'.format(task_id))
            try:
                run_label_to_id = get_artifact(task_id, "public/label-to-taskid.json")
                label_to_taskid.update(run_label_to_id)
            except HTTPError as e:
                if e.response.status_code != 404:
                    raise
                logger.debug('No label-to-taskid.json found for {}: {}'.format(task_id, e))

        namespace = '{}.v2.{}.revision.{}.cron'.format(
            graph_config['trust-domain'],
            parameters['project'],
            parameters['head_rev'])
        for task_id in list_tasks(namespace):
            fetches.append(e.submit(fetch_cron, task_id))

        # now wait for each fetch to complete, raising an exception if there
        # were any issues
        for f in futures.as_completed(fetches):
            f.result()

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


def update_dependencies(task, graph):
    if os.environ.get('TASK_ID'):
        task.task.setdefault('dependencies', []).append(os.environ['TASK_ID'])
    return task


def create_tasks(graph_config, to_run, full_task_graph, label_to_taskid,
                 params, decision_task_id=None, suffix='', modifier=lambda t: t):
    """Create new tasks.  The task definition will have {relative-datestamp':
    '..'} rendered just like in a decision task.  Action callbacks should use
    this function to create new tasks,
    allowing easy debugging with `mach taskgraph action-callback --test`.
    This builds up all required tasks to run in order to run the tasks requested.

    Optionally this function takes a `modifier` function that is passed in each
    task before it is put into a new graph. It should return a valid task. Note
    that this is passed _all_ tasks in the graph, not just the set in to_run. You
    may want to skip modifying tasks not in your to_run list.

    If `suffix` is given, then it is used to give unique names to the resulting
    artifacts.  If you call this function multiple times in the same action,
    pass a different suffix each time to avoid overwriting artifacts.

    If you wish to create the tasks in a new group, leave out decision_task_id.

    Returns an updated label_to_taskid containing the new tasks"""
    if suffix != '':
        suffix = '-{}'.format(suffix)
    to_run = set(to_run)

    #  Copy to avoid side-effects later
    full_task_graph = copy.deepcopy(full_task_graph)
    label_to_taskid = label_to_taskid.copy()

    target_graph = full_task_graph.graph.transitive_closure(to_run)
    target_task_graph = TaskGraph(
        {l: modifier(full_task_graph[l]) for l in target_graph.nodes},
        target_graph)
    target_task_graph.for_each_task(update_parent)
    if decision_task_id and decision_task_id != os.environ.get('TASK_ID'):
        target_task_graph.for_each_task(update_dependencies)
    optimized_task_graph, label_to_taskid = optimize_task_graph(target_task_graph,
                                                                params,
                                                                to_run,
                                                                label_to_taskid)
    write_artifact('task-graph{}.json'.format(suffix), optimized_task_graph.to_json())
    write_artifact('label-to-taskid{}.json'.format(suffix), label_to_taskid)
    write_artifact('to-run{}.json'.format(suffix), list(to_run))
    create.create_tasks(
        graph_config,
        optimized_task_graph,
        label_to_taskid,
        params,
        decision_task_id,
    )
    return label_to_taskid


def _update_reducer(accumulator, new_value):
    "similar to set or dict `update` method, but returning the modified object"
    accumulator.update(new_value)
    return accumulator


def combine_task_graph_files(suffixes):
    """Combine task-graph-{suffix}.json files into a single task-graph.json file.

    Since Chain of Trust verification requires a task-graph.json file that
    contains all children tasks, we can combine the various task-graph-0.json
    type files into a master task-graph.json file at the end.

    Actions also look for various artifacts, so we combine those in a similar
    fashion.

    In the case where there is only one suffix, we simply rename it to avoid the
    additional cost of uploading two copies of the same data.
    """

    if len(suffixes) == 1:
        for filename in ['task-graph', 'label-to-taskid', 'to-run']:
            rename_artifact(
                "{}-{}.json".format(filename, suffixes[0]),
                "{}.json".format(filename))
        return

    def combine(file_contents, base):
        return reduce(_update_reducer, file_contents, base)

    files = [read_artifact("task-graph-{}.json".format(suffix)) for suffix in suffixes]
    write_artifact("task-graph.json", combine(files, dict()))

    files = [read_artifact("label-to-taskid-{}.json".format(suffix)) for suffix in suffixes]
    write_artifact("label-to-taskid.json", combine(files, dict()))

    files = [read_artifact("to-run-{}.json".format(suffix)) for suffix in suffixes]
    write_artifact("to-run.json", list(combine(files, set())))


def relativize_datestamps(task_def):
    """
    Given a task definition as received from the queue, convert all datestamps
    to {relative_datestamp: ..} format, with the task creation time as "now".
    The result is useful for handing to ``create_task``.
    """
    base = parse_time(task_def['created'])
    # borrowed from https://github.com/epoberezkin/ajv/blob/master/lib/compile/formats.js
    ts_pattern = re.compile(
        r'^\d\d\d\d-[0-1]\d-[0-3]\d[t\s]'
        r'(?:[0-2]\d:[0-5]\d:[0-5]\d|23:59:60)(?:\.\d+)?'
        r'(?:z|[+-]\d\d:\d\d)$', re.I)

    def recurse(value):
        if isinstance(value, text_type):
            if ts_pattern.match(value):
                value = parse_time(value)
                diff = value - base
                return {'relative-datestamp': '{} seconds'.format(int(diff.total_seconds()))}
        if isinstance(value, list):
            return [recurse(e) for e in value]
        if isinstance(value, dict):
            return {k: recurse(v) for k, v in value.items()}
        return value
    return recurse(task_def)
