# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.optimize import OptimizationStrategy, register_strategy


@register_strategy("backstop")
class Backstop(OptimizationStrategy):
    """Ensures that no task gets left behind.

    Will schedule all tasks if this is a backstop push.
    """
    def should_remove_task(self, task, params, _):
        return not params["backstop"]


@register_strategy("push-interval-10", args=(10,))
@register_strategy("push-interval-20", args=(20,))
@register_strategy("push-interval-25", args=(25,))
class PushInterval(OptimizationStrategy):
    """Runs tasks every N pushes.

    Args:
        push_interval (int): Number of pushes
    """
    def __init__(self, push_interval):
        self.push_interval = push_interval

    def should_remove_task(self, task, params, _):
        # On every Nth push, want to run all tasks.
        return int(params["pushlog_id"]) % self.push_interval != 0
