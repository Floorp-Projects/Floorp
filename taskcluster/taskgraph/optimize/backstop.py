# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
from collections import defaultdict

import requests
from redo import retry

from taskgraph.optimize import OptimizationStrategy, register_strategy

logger = logging.getLogger(__name__)
PUSH_ENDPOINT = "{head_repository}/json-pushes/?startID={push_id_start}&endID={push_id_end}"


@register_strategy('backstop', args=(10, 60))
@register_strategy("push-interval-10", args=(10, 0))
@register_strategy("push-interval-25", args=(25, 0))
class Backstop(OptimizationStrategy):
    """Ensures that no task gets left behind.

    Will schedule all tasks either every Nth push, or M minutes.

    Args:
        push_interval (int): Number of pushes
        time_interval (int): Minutes between forced schedules.
                             Use 0 to disable.
    """
    def __init__(self, push_interval, time_interval):
        self.push_interval = push_interval
        self.time_interval = time_interval

        # cached push dates by project
        self.push_dates = defaultdict(dict)
        # cached push_ids that failed to retrieve datetime for
        self.failed_json_push_calls = []

    def should_remove_task(self, task, params, _):
        project = params['project']
        pushid = int(params['pushlog_id'])
        pushdate = int(params['pushdate'])

        # Only enable the backstop on autoland since we always want the *real*
        # optimized tasks on try and release branches.
        if project != 'autoland':
            return True

        # On every Nth push, want to run all tasks.
        if pushid % self.push_interval == 0:
            return False

        # We also want to ensure we run all tasks at least once per N minutes.
        if self.time_interval > 0 and self.minutes_between_pushes(
                params["head_repository"],
                project,
                pushid,
                pushdate) >= self.time_interval:
            return False
        return True

    def minutes_between_pushes(self, repository, project, cur_push_id, cur_push_date):
        # figure out the minutes that have elapsed between the current push and previous one
        # defaulting to max min so if we can't get value, defaults to run the task
        min_between_pushes = self.time_interval
        prev_push_id = cur_push_id - 1

        # cache the pushdate for the current push so we can use it next time
        self.push_dates[project].update({cur_push_id: cur_push_date})

        # check if we already have the previous push id's datetime cached
        prev_push_date = self.push_dates[project].get(prev_push_id, 0)

        # we have datetime of current and previous push, so return elapsed minutes and bail
        if cur_push_date > 0 and prev_push_date > 0:
            return (cur_push_date - prev_push_date) / 60

        # datetime for previous pushid not cached, so must retrieve it
        # if we already tried to retrieve the datetime for this pushid
        # before and the json-push request failed, don't try it again
        if prev_push_id in self.failed_json_push_calls:
            return min_between_pushes

        url = PUSH_ENDPOINT.format(
            head_repository=repository,
            push_id_start=prev_push_id - 1,
            push_id_end=prev_push_id,
        )

        try:
            response = retry(requests.get, attempts=2, sleeptime=10,
                             args=(url, ),
                             kwargs={'timeout': 60, 'headers': {'User-Agent': 'TaskCluster'}})
            prev_push_date = response.json().get(str(prev_push_id), {}).get('date', 0)

            # cache it for next time
            self.push_dates[project].update({prev_push_id: prev_push_date})

            # now have datetime of current and previous push
            if cur_push_date > 0 and prev_push_date > 0:
                min_between_pushes = (cur_push_date - prev_push_date) / 60

        # In the event of request times out, requests will raise a TimeoutError.
        except requests.exceptions.Timeout:
            logger.warning("json-pushes timeout, enabling backstop")
            self.failed_json_push_calls.append(prev_push_id)

        # In the event of a network problem (e.g. DNS failure, refused connection, etc),
        # requests will raise a ConnectionError.
        except requests.exceptions.ConnectionError:
            logger.warning("json-pushes connection error, enabling backstop")
            self.failed_json_push_calls.append(prev_push_id)

        # In the event of the rare invalid HTTP response(e.g 404, 401),
        # requests will raise an HTTPError exception
        except requests.exceptions.HTTPError:
            logger.warning("Bad Http response, enabling backstop")
            self.failed_json_push_calls.append(prev_push_id)

        # When we get invalid JSON (i.e. 500 error), it results in a ValueError (bug 1313426)
        except ValueError as error:
            logger.warning("Invalid JSON, possible server error: {}".format(error))
            self.failed_json_push_calls.append(prev_push_id)

        # We just print the error out as a debug message if we failed to catch the exception above
        except requests.exceptions.RequestException as error:
            logger.warning(error)
            self.failed_json_push_calls.append(prev_push_id)

        return min_between_pushes
