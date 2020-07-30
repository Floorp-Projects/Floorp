# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging

from taskgraph.util.hg import get_push_data


BACKSTOP_PUSH_INTERVAL = 10
BACKSTOP_TIME_INTERVAL = 60  # minutes

logger = logging.getLogger(__name__)


def is_backstop(
    params, push_interval=BACKSTOP_PUSH_INTERVAL, time_interval=BACKSTOP_TIME_INTERVAL
):
    """Determines whether the given parameters represent a backstop push.

    Args:
        push_interval (int): Number of pushes
        time_interval (int): Minutes between forced schedules.
                             Use 0 to disable.
    Returns:
        bool: True if this is a backtop, otherwise False.
    """
    project = params["project"]
    pushid = int(params["pushlog_id"])
    pushdate = int(params["pushdate"])

    if project != "autoland":
        return False

    # On every Nth push, want to run all tasks.
    if pushid % push_interval == 0:
        return True

    # We also want to ensure we run all tasks at least once per N minutes.
    if (
        time_interval > 0
        and minutes_between_pushes(
            time_interval, params["head_repository"], project, pushid, pushdate
        )
        >= time_interval
    ):
        return True
    return False


def minutes_between_pushes(time_interval, repository, project, cur_push_id, cur_push_date):
    # figure out the minutes that have elapsed between the current push and previous one
    # defaulting to max min so if we can't get value, defaults to run the task
    min_between_pushes = time_interval
    prev_push_id = cur_push_id - 1

    data = get_push_data(repository, project, prev_push_id, prev_push_id)

    if data is not None:
        prev_push_date = data[prev_push_id].get('date', 0)

        # now have datetime of current and previous push
        if cur_push_date > 0 and prev_push_date > 0:
            min_between_pushes = (cur_push_date - prev_push_date) / 60

    return min_between_pushes
