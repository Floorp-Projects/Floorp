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
    """Filter out tasks for Servo vendoring changesets.

    If the change triggering is related to Servo vendoring, impact is minimal
    because not all platforms use Servo code.

    We filter out tests on platforms that don't run Servo tests because running
    tests will accomplish little for these changes.
    """

    SERVO_TEST_PLATFORMS = {
        'linux64',
        'linux64-stylo',
    }

    def fltr(task):
        if parameters.get('owner') != "servo-vcs-sync@mozilla.com":
            return True

        # This is a Servo vendor change.

        # Servo code is compiled. So we at least need to build. Resource
        # savings come from pruning tests. So that's where we filter.
        if task.attributes.get('kind') != 'test':
            return True

        return task.attributes.get('build_platform') in SERVO_TEST_PLATFORMS

    return [l for l, t in graph.tasks.iteritems() if fltr(t)]
