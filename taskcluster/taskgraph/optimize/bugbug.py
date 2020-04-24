# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import logging
import time

import requests
from mozbuild.util import memoize, memoized_property
from six.moves.urllib.parse import urlsplit

from taskgraph.optimize import register_strategy, OptimizationStrategy
from taskgraph.util.taskcluster import requests_retry_session

logger = logging.getLogger(__name__)

# Preset confidence thresholds.
CT_LOW = 0.5
CT_MEDIUM = 0.7
CT_HIGH = 0.9


class BugbugTimeoutException(Exception):
    pass


@register_strategy("platform-debug")
class SkipUnlessDebug(OptimizationStrategy):
    """Only run debug platforms."""

    def should_remove_task(self, task, params, arg):
        return not (task.attributes.get('build_type') == "debug")


@register_strategy("bugbug", args=(CT_MEDIUM,))
@register_strategy("bugbug-combined-high", args=(CT_HIGH, False, True))
@register_strategy("bugbug-low", args=(CT_LOW,))
@register_strategy("bugbug-high", args=(CT_HIGH,))
@register_strategy("bugbug-reduced", args=(CT_MEDIUM, True))
@register_strategy("bugbug-reduced-high", args=(CT_HIGH, True))
class BugBugPushSchedules(OptimizationStrategy):
    """Query the 'bugbug' service to retrieve relevant tasks and manifests.

    Args:
        confidence_threshold (float): The minimum confidence threshold (in
            range [0, 1]) needed for a task to be scheduled.
        use_reduced_tasks (bool): Whether or not to use the reduced set of tasks
            provided by the bugbug service (default: False).
        combine_weights (bool): If True, sum the confidence thresholds of all
            groups within a task to find the overall task confidence. Otherwise
            the maximum confidence threshold is used (default: False).
    """
    BUGBUG_BASE_URL = "https://bugbug.herokuapp.com"
    RETRY_TIMEOUT = 4 * 60  # seconds
    RETRY_INTERVAL = 5      # seconds

    def __init__(self, confidence_threshold, use_reduced_tasks=False, combine_weights=False):
        self.confidence_threshold = confidence_threshold
        self.use_reduced_tasks = use_reduced_tasks
        self.combine_weights = combine_weights

    @memoized_property
    def session(self):
        s = requests.Session()
        s.headers.update({'X-API-KEY': 'gecko-taskgraph'})
        return requests_retry_session(retries=5, session=s)

    @memoize
    def run_query(self, query, data=None):
        url = self.BUGBUG_BASE_URL + query

        attempts = self.RETRY_TIMEOUT / self.RETRY_INTERVAL
        i = 0
        while i < attempts:
            r = self.session.get(url)
            r.raise_for_status()

            if r.status_code != 202:
                break

            time.sleep(self.RETRY_INTERVAL)
            i += 1

        data = r.json()
        logger.debug("Bugbug scheduler service returns:\n{}".format(
                     json.dumps(data, indent=2)))

        if r.status_code == 202:
            raise BugbugTimeoutException("Timed out waiting for result from '{}'".format(url))

        return data

    def should_remove_task(self, task, params, arg):
        branch = urlsplit(params['head_repository']).path.strip('/')
        rev = params['head_rev']
        data = self.run_query('/push/{branch}/{rev}/schedules'.format(branch=branch, rev=rev))

        if not self.use_reduced_tasks:
            tasks = set(
                task
                for task, confidence in data.get('tasks', {}).items()
                if confidence >= self.confidence_threshold
            )
        else:
            tasks = set(
                task
                for task, confidence in data.get('reduced_tasks', {}).items()
                if confidence >= self.confidence_threshold
            )

        test_manifests = task.attributes.get('test_manifests')
        if test_manifests is None or self.use_reduced_tasks:
            if task.label not in tasks:
                return True

            return False

        # If a task contains more than one group, figure out which confidence
        # threshold to use. If 'self.combine_weights' is set, add up all
        # confidence thresholds. Otherwise just use the max.
        task_confidence = 0
        for group, confidence in data.get("groups", {}).items():
            if group not in test_manifests:
                continue

            if self.combine_weights:
                task_confidence = round(
                    task_confidence + confidence - task_confidence * confidence, 2
                )
            else:
                task_confidence = max(task_confidence, confidence)

        return task_confidence < self.confidence_threshold
