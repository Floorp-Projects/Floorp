# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.optimize import OptimizationStrategy, register_strategy
from taskgraph.util.attributes import match_run_on_projects


@register_strategy("backstop")
class Backstop(OptimizationStrategy):
    """Ensures that no task gets left behind.

    Will schedule all tasks if this is a backstop push.
    """
    def should_remove_task(self, task, params, _):
        return not params["backstop"]


@register_strategy("push-interval-10", args=(10,))
@register_strategy("push-interval-5", args=(5,))
@register_strategy("push-interval-20", args=(20,))
class PushInterval(OptimizationStrategy):
    """Runs tasks every N pushes.

    Args:
        push_interval (int): Number of pushes
        remove_on_projects (set): For non-autoland projects, the task will
            be removed if we're running on one of these projects, otherwise
            it will be kept.
    """
    def __init__(self, push_interval, remove_on_projects=None):
        self.push_interval = push_interval
        self.remove_on_projects = remove_on_projects or {'try'}

    def should_remove_task(self, task, params, _):
        project = params["project"]

        # Scheduling on a backstop only makes sense on autoland. For other projects,
        # remove the task if the project matches self.remove_on_projects.
        if project != 'autoland':
            return match_run_on_projects(project, self.remove_on_projects)

        # On every Nth push, want to run all tasks.
        return int(params["pushlog_id"]) % self.push_interval != 0
