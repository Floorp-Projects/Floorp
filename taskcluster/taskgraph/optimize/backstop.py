# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.optimize import OptimizationStrategy, register_strategy
from taskgraph.util.attributes import match_run_on_projects
from taskgraph.util.backstop import is_backstop, BACKSTOP_PUSH_INTERVAL, BACKSTOP_TIME_INTERVAL


@register_strategy('backstop', args=(BACKSTOP_PUSH_INTERVAL, BACKSTOP_TIME_INTERVAL, {'all'}))
@register_strategy("push-interval-10", args=(10, 0, {'try'}))
@register_strategy("push-interval-25", args=(25, 0, {'try'}))
class Backstop(OptimizationStrategy):
    """Ensures that no task gets left behind.

    Will schedule all tasks either every Nth push, or M minutes. This behaviour
    is only enabled on autoland. For all other projects, the
    `remove_on_projects` flag determines what will happen.

    Args:
        push_interval (int): Number of pushes
        time_interval (int): Minutes between forced schedules.
                             Use 0 to disable.
        remove_on_projects (set): For non-autoland projects, the task will
            be removed if we're running on one of these projects, otherwise
            it will be kept.
    """
    def __init__(self, push_interval, time_interval, remove_on_projects):
        self.push_interval = push_interval
        self.time_interval = time_interval
        self.remove_on_projects = remove_on_projects

    def should_remove_task(self, task, params, _):
        project = params["project"]

        # Scheduling on a backstop only makes sense on autoland. For other projects,
        # remove the task if the project matches self.remove_on_projects.
        if project != 'autoland':
            return match_run_on_projects(project, self.remove_on_projects)

        if is_backstop(params, self.push_interval, self.time_interval):
            return False
        return True
