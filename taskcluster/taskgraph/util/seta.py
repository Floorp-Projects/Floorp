# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import logging
import requests
from collections import defaultdict
from redo import retry
from requests import exceptions

logger = logging.getLogger(__name__)

# It's a list of project name which SETA is useful on
SETA_PROJECTS = ['mozilla-inbound', 'autoland']
PROJECT_SCHEDULE_ALL_EVERY_PUSHES = {'mozilla-inbound': 5, 'autoland': 5}
PROJECT_SCHEDULE_ALL_EVERY_MINUTES = {'mozilla-inbound': 60, 'autoland': 60}

SETA_ENDPOINT = "https://treeherder.mozilla.org/api/project/%s/seta/" \
                "job-priorities/?build_system_type=%s"
PUSH_ENDPOINT = "https://hg.mozilla.org/integration/%s/json-pushes/?startID=%d&endID=%d"


class SETA(object):
    """
    Interface to the SETA service, which defines low-value tasks that can be optimized out
    of the taskgraph.
    """
    def __init__(self):
        # cached low value tasks, by project
        self.low_value_tasks = {}
        self.low_value_bb_tasks = {}
        # cached push dates by project
        self.push_dates = defaultdict(dict)
        # cached push_ids that failed to retrieve datetime for
        self.failed_json_push_calls = []

    def _get_task_string(self, task_tuple):
        # convert task tuple to single task string, so the task label sent in can match
        # remove any empty parts of the tuple
        task_tuple = [x for x in task_tuple if len(x) != 0]

        if len(task_tuple) == 0:
            return ''
        if len(task_tuple) != 3:
            return ' '.join(task_tuple)

        return 'test-%s/%s-%s' % (task_tuple[0], task_tuple[1], task_tuple[2])

    def query_low_value_tasks(self, project):
        # Request the set of low value tasks from the SETA service.  Low value
        # tasks will be optimized out of the task graph.
        low_value_tasks = []

        # we want to get low priority taskcluster jobs
        url = SETA_ENDPOINT % (project, 'taskcluster')

        # Try to fetch the SETA data twice, falling back to an empty list of low value tasks.
        # There are 10 seconds between each try.
        try:
            logger.debug("Retrieving low-value jobs list from SETA")
            response = retry(requests.get, attempts=2, sleeptime=10,
                             args=(url, ),
                             kwargs={'timeout': 60, 'headers': ''})
            task_list = json.loads(response.content).get('jobtypes', '')

            if type(task_list) == dict and len(task_list) > 0:
                if type(task_list.values()[0]) == list and len(task_list.values()[0]) > 0:
                    low_value_tasks = task_list.values()[0]
                    # bb job types return a list instead of a single string,
                    # convert to a single string to match tc tasks format
                    if type(low_value_tasks[0]) == list:
                        low_value_tasks = [self._get_task_string(x) for x in low_value_tasks]

            # ensure no build tasks slipped in, we never want to optimize out those
            low_value_tasks = [x for x in low_value_tasks if 'build' not in x.lower()]

        # In the event of request times out, requests will raise a TimeoutError.
        except exceptions.Timeout:
            logger.warning("SETA timeout, we will treat all test tasks as high value.")

        # In the event of a network problem (e.g. DNS failure, refused connection, etc),
        # requests will raise a ConnectionError.
        except exceptions.ConnectionError:
            logger.warning("SETA connection error, we will treat all test tasks as high value.")

        # In the event of the rare invalid HTTP response(e.g 404, 401),
        # requests will raise an HTTPError exception
        except exceptions.HTTPError:
            logger.warning("We got bad Http response from ouija,"
                           " we will treat all test tasks as high value.")

        # We just print the error out as a debug message if we failed to catch the exception above
        except exceptions.RequestException as error:
            logger.warning(error)

        # When we get invalid JSON (i.e. 500 error), it results in a ValueError (bug 1313426)
        except ValueError as error:
            logger.warning("Invalid JSON, possible server error: {}".format(error))

        return low_value_tasks

    def minutes_between_pushes(self, project, cur_push_id, cur_push_date):
        # figure out the minutes that have elapsed between the current push and previous one
        # defaulting to max min so if we can't get value, defaults to run the task
        min_between_pushes = PROJECT_SCHEDULE_ALL_EVERY_MINUTES.get(project, 60)
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

        url = PUSH_ENDPOINT % (project, cur_push_id - 2, prev_push_id)

        try:
            response = retry(requests.get, attempts=2, sleeptime=10,
                             args=(url, ),
                             kwargs={'timeout': 60, 'headers': {'User-Agent': 'TaskCluster'}})
            prev_push_date = json.loads(response.content).get(str(prev_push_id), {}).get('date', 0)

            # cache it for next time
            self.push_dates[project].update({prev_push_id: prev_push_date})

            # now have datetime of current and previous push
            if cur_push_date > 0 and prev_push_date > 0:
                min_between_pushes = (cur_push_date - prev_push_date) / 60

        # In the event of request times out, requests will raise a TimeoutError.
        except exceptions.Timeout:
            logger.warning("json-pushes timeout, treating task as high value")
            self.failed_json_push_calls.append(prev_push_id)

        # In the event of a network problem (e.g. DNS failure, refused connection, etc),
        # requests will raise a ConnectionError.
        except exceptions.ConnectionError:
            logger.warning("json-pushes connection error, treating task as high value")
            self.failed_json_push_calls.append(prev_push_id)

        # In the event of the rare invalid HTTP response(e.g 404, 401),
        # requests will raise an HTTPError exception
        except exceptions.HTTPError:
            logger.warning("Bad Http response, treating task as high value")
            self.failed_json_push_calls.append(prev_push_id)

        # When we get invalid JSON (i.e. 500 error), it results in a ValueError (bug 1313426)
        except ValueError as error:
            logger.warning("Invalid JSON, possible server error: {}".format(error))
            self.failed_json_push_calls.append(prev_push_id)

        # We just print the error out as a debug message if we failed to catch the exception above
        except exceptions.RequestException as error:
            logger.warning(error)
            self.failed_json_push_calls.append(prev_push_id)

        return min_between_pushes

    def is_low_value_task(self, label, project, pushlog_id, push_date):
        # marking a task as low_value means it will be optimized out by tc
        if project not in SETA_PROJECTS:
            return False

        schedule_all_every = PROJECT_SCHEDULE_ALL_EVERY_PUSHES.get(project, 5)
        # on every Nth push, want to run all tasks
        if int(pushlog_id) % schedule_all_every == 0:
            return False

        # Nth push, so time to call seta based on number of pushes; however
        # we also want to ensure we run all tasks at least once per N minutes
        if self.minutes_between_pushes(
                project,
                int(pushlog_id),
                int(push_date)) >= PROJECT_SCHEDULE_ALL_EVERY_MINUTES.get(project, 60):
            return False

        # cache the low value tasks per project to avoid repeated SETA server queries
        if project not in self.low_value_tasks:
            self.low_value_tasks[project] = self.query_low_value_tasks(project)
        return label in self.low_value_tasks[project]


# create a single instance of this class, and expose its `is_low_value_task`
# bound method as a module-level function
is_low_value_task = SETA().is_low_value_task
