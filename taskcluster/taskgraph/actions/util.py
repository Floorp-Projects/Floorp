# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import sys

from taskgraph import create
from taskgraph.util.taskcluster import get_session, find_task_id

# this is set to true for `mach taskgraph action-callback --test`
testing = False


def find_decision_task(parameters):
    """Given the parameters for this action, find the taskId of the decision
    task"""
    return find_task_id('gecko.v2.{}.pushlog-id.{}.decision'.format(
        parameters['project'],
        parameters['pushlog_id']))


def create_task(task_id, task_def):
    """Create a new task.  The task definition will have {relative-datestamp':
    '..'} rendered just like in a decision task.  Action callbacks should use
    this function to create new tasks, as it has the additional advantage of
    allowing easy debugging with `mach taskgraph action-callback --test`."""
    if testing:
        json.dump([task_id, task_def], sys.stdout,
                  sort_keys=True, indent=4, separators=(',', ': '))
        return
    label = task_def['metadata']['name']
    session = get_session()
    create.create_task(session, task_id, label, task_def)
