# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .registry import register_callback_action

from .util import (
    create_tasks,
    fetch_graph_and_labels,
)


@register_callback_action(
    name='openh264',
    title='OpenH264 Binaries',
    symbol='h264',
    description="Action to prepare openh264 binaries for shipping",
    context=[],
)
def openh264_action(parameters, graph_config, input, task_group_id, task_id):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config)
    to_run = [label
              for label, entry
              in full_task_graph.tasks.iteritems() if 'openh264' in entry.kind]
    create_tasks(
        graph_config,
        to_run,
        full_task_graph,
        label_to_taskid,
        parameters,
        decision_task_id,
    )
