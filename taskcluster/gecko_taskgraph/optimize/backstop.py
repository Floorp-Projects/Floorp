# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.optimize.base import All, OptimizationStrategy, register_strategy

from gecko_taskgraph.util.backstop import BACKSTOP_PUSH_INTERVAL


@register_strategy("skip-unless-backstop")
class SkipUnlessBackstop(OptimizationStrategy):
    """Always removes tasks except on backstop pushes."""

    def should_remove_task(self, task, params, _):
        return not params["backstop"]


class SkipUnlessPushInterval(OptimizationStrategy):
    """Always removes tasks except every N pushes.

    Args:
        push_interval (int): Number of pushes
    """

    def __init__(self, push_interval, remove_on_projects=None):
        self.push_interval = push_interval

    @property
    def description(self):
        return f"skip-unless-push-interval-{self.push_interval}"

    def should_remove_task(self, task, params, _):
        # On every Nth push, want to run all tasks.
        return int(params["pushlog_id"]) % self.push_interval != 0


# Strategy to run tasks on "expanded" pushes, currently defined as pushes that
# are half the backstop interval. The 'All' composite strategy means that the
# "backstop" strategy will prevent "expanded" from applying on backstop pushes.
register_strategy(
    "skip-unless-expanded",
    args=(
        "skip-unless-backstop",
        SkipUnlessPushInterval(BACKSTOP_PUSH_INTERVAL / 2),
    ),
)(All)
