# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import logging

from . import (
    target_tasks,
)

logger = logging.getLogger(__name__)

filter_task_functions = {}


def filter_task(name):
    """Generator to declare a task filter function."""
    def wrap(func):
        filter_task_functions[name] = func
        return func
    return wrap


@filter_task('target_tasks_method')
def filter_target_tasks(graph, parameters):
    """Proxy filter to use legacy target tasks code.

    This should go away once target_tasks are converted to filters.
    """

    attr = parameters.get('target_tasks_method', 'all_tasks')
    fn = target_tasks.get_method(attr)
    return fn(graph, parameters)


@filter_task('check_servo')
def filter_servo(graph, parameters):
    """bug 1339542 When Servo vcs sync service lands Servo commits in
    autoland repo, run linux64-stylo tests but skip other
    platforms (to  reduce test load)."""

    SERVO_PLATFORMS = {
        'linux64',
        'linux64-stylo',
    }

    def fltr(task):
        if parameters.get('owner') != "servo-vcs-sync@mozilla.com":
            return True
        return task.attributes.get('build_platform') in SERVO_PLATFORMS

    return [l for l, t in graph.tasks.iteritems() if fltr(t)]
