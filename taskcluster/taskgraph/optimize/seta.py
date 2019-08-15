# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import logging
import requests
from collections import defaultdict

import attr
from redo import retry
from requests import exceptions

from taskgraph.optimize import OptimizationStrategy, register_strategy

logger = logging.getLogger(__name__)

# It's a list of project name which SETA is useful on
SETA_PROJECTS = ['mozilla-inbound', 'autoland']
SETA_HIGH_PRIORITY = 1
SETA_LOW_PRIORITY = 5

SETA_ENDPOINT = "https://treeherder.mozilla.org/api/project/%s/seta/" \
                "job-priorities/?build_system_type=%s&priority=%s"
PUSH_ENDPOINT = "https://hg.mozilla.org/integration/%s/json-pushes/?startID=%d&endID=%d"


@attr.s(frozen=True)
class SETA(object):
    """
    Interface to the SETA service, which defines low-value tasks that can be optimized out
    of the taskgraph.
    """

    # cached low value tasks, by project
    low_value_tasks = attr.ib(factory=dict, init=False)
    low_value_bb_tasks = attr.ib(factory=dict, init=False)
    # cached push dates by project
    push_dates = attr.ib(factory=lambda: defaultdict(dict), init=False)
    # cached push_ids that failed to retrieve datetime for
    failed_json_push_calls = attr.ib(factory=list, init=False)

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
        url_low = SETA_ENDPOINT % (project, 'taskcluster', SETA_LOW_PRIORITY)
        url_high = SETA_ENDPOINT % (project, 'taskcluster', SETA_HIGH_PRIORITY)

        # Try to fetch the SETA data twice, falling back to an empty list of low value tasks.
        # There are 10 seconds between each try.
        try:
            logger.debug("Retrieving low-value jobs list from SETA")
            response = retry(requests.get, attempts=2, sleeptime=10,
                             args=(url_low, ),
                             kwargs={'timeout': 60, 'headers': ''})
            task_list = json.loads(response.content).get('jobtypes', '')

            if type(task_list) == dict and len(task_list) > 0:
                if type(task_list.values()[0]) == list and len(task_list.values()[0]) > 0:
                    low_value_tasks = set(task_list.values()[0])

            # hack seta tasks to run 'opt' jobs on 'pgo' builds - see Bug 1522111
            logger.debug("Retrieving high-value jobs list from SETA")
            response = retry(requests.get, attempts=2, sleeptime=10,
                             args=(url_high, ),
                             kwargs={'timeout': 60, 'headers': ''})
            task_list = json.loads(response.content).get('jobtypes', '')

            high_value_tasks = set([])
            if type(task_list) == dict and len(task_list) > 0:
                if type(task_list.values()[0]) == list and len(task_list.values()[0]) > 0:
                    high_value_tasks = set(task_list.values()[0])

            # hack seta to treat all Android Raptor tasks as low value - see Bug 1535016
            def only_android_raptor(task):
                return task.startswith('test-android') and 'raptor' in task

            high_value_android_tasks = list(filter(only_android_raptor, high_value_tasks))
            low_value_tasks.update(high_value_android_tasks)

            seta_conversions = {
                # old: new
                'test-linux32/opt': 'test-linux32-shippable/opt',
                'test-linux64/opt': 'test-linux64-shippable/opt',
                'test-linux64-pgo/opt': 'test-linux64-shippable/opt',
                'test-linux64-pgo-qr/opt': 'test-linux64-shippable-qr/opt',
                'test-linux64-qr/opt': 'test-linux64-shippable-qr/opt',
                'test-windows7-32/opt': 'test-windows7-32-shippable/opt',
                'test-windows7-32-pgo/opt': 'test-windows7-32-shippable/opt',
                'test-windows10-64/opt': 'test-windows10-64-shippable/opt',
                'test-windows10-64-pgo/opt': 'test-windows10-64-shippable/opt',
                'test-windows10-64-pgo-qr/opt': 'test-windows10-64-shippable-qr/opt',
                'test-windows10-64-qr/opt': 'test-windows10-64-shippable-qr/opt',
                }
            # Now add new variants to the low-value set
            for old, new in seta_conversions.iteritems():
                if any(t.startswith(old) for t in low_value_tasks):
                    low_value_tasks.update(
                        [t.replace(old, new) for t in low_value_tasks]
                    )

            # ... and the high value list
            for old, new in seta_conversions.iteritems():
                if any(t.startswith(old) for t in high_value_tasks):
                    high_value_tasks.update(
                        [t.replace(old, new) for t in high_value_tasks]
                    )

            def new_as_old_is_high_value(label):
                # This doesn't care if there are multiple old values for one new
                # it will always check every old value.
                for old, new in seta_conversions.iteritems():
                    if label.startswith(new):
                        old_label = label.replace(new, old)
                        if old_label in high_value_tasks:
                            return True
                return False

            # Now rip out from low value things that were high value in opt
            low_value_tasks = set([x for x in low_value_tasks if not new_as_old_is_high_value(x)])

            # ensure no build tasks slipped in, we never want to optimize out those
            low_value_tasks = set([x for x in low_value_tasks if 'build' not in x])

            # Strip out any duplicates from the above conversions
            low_value_tasks = list(set(low_value_tasks))

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

    def minutes_between_pushes(self, project, cur_push_id, cur_push_date, time_interval):
        # figure out the minutes that have elapsed between the current push and previous one
        # defaulting to max min so if we can't get value, defaults to run the task
        min_between_pushes = time_interval
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

    def is_low_value_task(self, label, project, pushlog_id, push_date,
                          push_interval, time_interval):
        # marking a task as low_value means it will be optimized out by tc
        if project not in SETA_PROJECTS:
            return False

        # on every Nth push, want to run all tasks
        if int(pushlog_id) % push_interval == 0:
            return False

        # Nth push, so time to call seta based on number of pushes; however
        # we also want to ensure we run all tasks at least once per N minutes
        if self.minutes_between_pushes(
                project,
                int(pushlog_id),
                int(push_date),
                time_interval) >= time_interval:
            return False

        # cache the low value tasks per project to avoid repeated SETA server queries
        if project not in self.low_value_tasks:
            self.low_value_tasks[project] = self.query_low_value_tasks(project)
        return label in self.low_value_tasks[project]


# create a single instance of this class, and expose its `is_low_value_task`
# bound method as a module-level function
is_low_value_task = SETA().is_low_value_task


@register_strategy('seta', args=(5, 60))
@register_strategy('seta_10_120', args=(10, 120))
class SkipLowValue(OptimizationStrategy):

    def __init__(self, push_interval, time_interval):
        self.push_interval = push_interval
        self.time_interval = time_interval

    def should_remove_task(self, task, params, _):
        label = task.label

        # we would like to return 'False, None' while it's high_value_task
        # and we wouldn't optimize it. Otherwise, it will return 'True, None'
        if is_low_value_task(label,
                             params.get('project'),
                             params.get('pushlog_id'),
                             params.get('pushdate'),
                             self.push_interval,
                             self.time_interval):
            # Always optimize away low-value tasks
            return True
        else:
            return False
