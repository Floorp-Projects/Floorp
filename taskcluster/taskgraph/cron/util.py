# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals


def match_utc(params, sched):
    """Return True if params['time'] matches the given schedule.

    If minute is not specified, then every multiple of fifteen minutes will match.
    Times not an even multiple of fifteen minutes will result in an exception
    (since they would never run).
    If hour is not specified, any hour will match. Similar for day and weekday.
    """
    if sched.get('minute') and sched.get('minute') % 15 != 0:
        raise Exception("cron jobs only run on multiples of 15 minutes past the hour")

    if sched.get('minute') is not None and sched.get('minute') != params['time'].minute:
        return False

    if sched.get('hour') is not None and sched.get('hour') != params['time'].hour:
        return False

    if sched.get('day') is not None and sched.get('day') != params['time'].day:
        return False

    if isinstance(sched.get('weekday'), str) or isinstance(sched.get('weekday'), unicode):
        if sched.get('weekday', str()).lower() != params['time'].strftime('%A').lower():
            return False
    elif sched.get('weekday') is not None:
        # don't accept other values.
        return False

    return True
