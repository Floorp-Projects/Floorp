# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging

from .registry import register_callback_action
from .util import create_tasks, fetch_graph_and_labels
from taskgraph.util.taskcluster import get_artifact

logger = logging.getLogger(__name__)


@register_callback_action(
    name='run-missing-tests',
    title='Run Missing Tests',
    kind='hook',
    generic=True,
    symbol='rmt',
    description=(
        "Run tests in the selected push that were optimized away, usually by SETA."
        "\n"
        "This action is for use on pushes that will be merged into another branch,"
        "to check that optimization hasn't hidden any failures."
    ),
    order=100,  # Useful for sheriffs, but not top of the list
    context=[],  # Applies to decision task
)
def run_missing_tests(parameters, graph_config, input, task_group_id, task_id, task):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config)
    target_tasks = get_artifact(decision_task_id, "public/target-tasks.json")

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
        to_run.append(label)

    create_tasks(to_run, full_task_graph, label_to_taskid, parameters, decision_task_id)

    logger.info('Out of {} test tasks, {} already existed and the action created {}'.format(
        already_run + len(to_run), already_run, len(to_run)))
