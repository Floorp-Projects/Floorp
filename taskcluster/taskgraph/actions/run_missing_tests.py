# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
from slugid import nice as slugid

from .registry import register_callback_action
from .util import create_task, find_decision_task
from taskgraph.util.taskcluster import get_artifact
from taskgraph.util.parameterization import resolve_task_references
from taskgraph.taskgraph import TaskGraph

logger = logging.getLogger(__name__)


@register_callback_action(
    name='run-missing-tests',
    title='Run Missing Tests',
    symbol='rmt',
    description="""
    Run tests in the selected push that were optimized away, usually by SETA.

    This action is for use on pushes that will be merged into another branch,
    to check that optimization hasn't hidden any failures.
    """,
    order=100,  # Useful for sheriffs, but not top of the list
    context=[],  # Applies to any task
)
def run_missing_tests(parameters, input, task_group_id, task_id, task):
    decision_task_id = find_decision_task(parameters)

    full_task_graph = get_artifact(decision_task_id, "public/full-task-graph.json")
    _, full_task_graph = TaskGraph.from_json(full_task_graph)
    target_tasks = get_artifact(decision_task_id, "public/target-tasks.json")
    label_to_taskid = get_artifact(decision_task_id, "public/label-to-taskid.json")

    # The idea here is to schedule all tasks of the `test` kind that were
    # targetted but did not appear in the final task-graph -- those were the
    # optimized tasks.
    to_run = []
    already_run = 0
    for label in target_tasks:
        task = full_task_graph.tasks[label]
        if task.kind != 'test':
            continue  # not a test
        if label in label_to_taskid:
            already_run += 1
            continue
        to_run.append(task)

    for task in to_run:

        # fix up the task's dependencies, similar to how optimization would
        # have done in the decision
        dependencies = {name: label_to_taskid[label]
                        for name, label in task.dependencies.iteritems()}
        task_def = resolve_task_references(task.label, task.task, dependencies)
        task_def.setdefault('dependencies', []).extend(dependencies.itervalues())
        create_task(slugid(), task_def, parameters['level'])

    logger.info('Out of {} test tasks, {} already existed and the action created {}'.format(
        already_run + len(to_run), already_run, len(to_run)))
