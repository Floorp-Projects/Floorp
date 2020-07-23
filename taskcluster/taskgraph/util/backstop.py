# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
from collections import defaultdict

import requests
from mozbuild.util import memoize
from redo import retry

BACKSTOP_PUSH_INTERVAL = 10
BACKSTOP_TIME_INTERVAL = 60  # minutes
PUSH_ENDPOINT = (
    "{head_repository}/json-pushes/?startID={push_id_start}&endID={push_id_end}"
)

# cached push dates by project
PUSH_DATES = defaultdict(dict)
# cached push_ids that failed to retrieve datetime for
FAILED_JSON_PUSH_CALLS = []

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


@memoize
def minutes_between_pushes(
    time_interval, repository, project, cur_push_id, cur_push_date
):
    # figure out the minutes that have elapsed between the current push and previous one
    # defaulting to max min so if we can't get value, defaults to run the task
    min_between_pushes = time_interval
    prev_push_id = cur_push_id - 1

    # cache the pushdate for the current push so we can use it next time
    PUSH_DATES[project].update({cur_push_id: cur_push_date})

    # check if we already have the previous push id's datetime cached
    prev_push_date = PUSH_DATES[project].get(prev_push_id, 0)

    # we have datetime of current and previous push, so return elapsed minutes and bail
    if cur_push_date > 0 and prev_push_date > 0:
        return (cur_push_date - prev_push_date) / 60

    # datetime for previous pushid not cached, so must retrieve it
    # if we already tried to retrieve the datetime for this pushid
    # before and the json-push request failed, don't try it again
    if prev_push_id in FAILED_JSON_PUSH_CALLS:
        return min_between_pushes

    url = PUSH_ENDPOINT.format(
        head_repository=repository,
        push_id_start=prev_push_id - 1,
        push_id_end=prev_push_id,
    )

    try:
        response = retry(
            requests.get,
            attempts=2,
            sleeptime=10,
            args=(url,),
            kwargs={"timeout": 60, "headers": {"User-Agent": "TaskCluster"}},
        )
        prev_push_date = response.json().get(str(prev_push_id), {}).get("date", 0)

        # cache it for next time
        PUSH_DATES[project].update({prev_push_id: prev_push_date})

        # now have datetime of current and previous push
        if cur_push_date > 0 and prev_push_date > 0:
            min_between_pushes = (cur_push_date - prev_push_date) / 60

    # In the event of request times out, requests will raise a TimeoutError.
    except requests.exceptions.Timeout:
        logger.warning("json-pushes timeout, enabling backstop")
        FAILED_JSON_PUSH_CALLS.append(prev_push_id)

    # In the event of a network problem (e.g. DNS failure, refused connection, etc),
    # requests will raise a ConnectionError.
    except requests.exceptions.ConnectionError:
        logger.warning("json-pushes connection error, enabling backstop")
        FAILED_JSON_PUSH_CALLS.append(prev_push_id)

    # In the event of the rare invalid HTTP response(e.g 404, 401),
    # requests will raise an HTTPError exception
    except requests.exceptions.HTTPError:
        logger.warning("Bad Http response, enabling backstop")
        FAILED_JSON_PUSH_CALLS.append(prev_push_id)

    # When we get invalid JSON (i.e. 500 error), it results in a ValueError (bug 1313426)
    except ValueError as error:
        logger.warning("Invalid JSON, possible server error: {}".format(error))
        FAILED_JSON_PUSH_CALLS.append(prev_push_id)

    # We just print the error out as a debug message if we failed to catch the exception above
    except requests.exceptions.RequestException as error:
        logger.warning(error)
        FAILED_JSON_PUSH_CALLS.append(prev_push_id)

    return min_between_pushes
