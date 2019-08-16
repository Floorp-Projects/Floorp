# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os

from mozbuild.base import MozbuildObject
from mozbuild.util import memoize

from taskgraph import files_changed
from taskgraph.optimize import register_strategy, OptimizationStrategy
from taskgraph.util.seta import is_low_value_task
from taskgraph.util.taskcluster import find_task_id

logger = logging.getLogger(__name__)


@register_strategy("index-search")
class IndexSearch(OptimizationStrategy):

    # A task with no dependencies remaining after optimization will be replaced
    # if artifacts exist for the corresponding index_paths.
    # Otherwise, we're in one of the following cases:
    # - the task has un-optimized dependencies
    # - the artifacts have expired
    # - some changes altered the index_paths and new artifacts need to be
    # created.
    # In every of those cases, we need to run the task to create or refresh
    # artifacts.

    def should_replace_task(self, task, params, index_paths):
        "Look for a task with one of the given index paths"
        for index_path in index_paths:
            try:
                task_id = find_task_id(
                    index_path,
                    use_proxy=bool(os.environ.get('TASK_ID')))
                return task_id
            except KeyError:
                # 404 will end up here and go on to the next index path
                pass

        return False


@register_strategy('seta')
class SETA(OptimizationStrategy):
    push_interval = 5
    time_interval = 60

    def should_remove_task(self, task, params, _):
        label = task.label

        # we would like to return 'False, None' while it's high_value_task
        # and we wouldn't optimize it. Otherwise, it will return 'True, None'
        if is_low_value_task(label,
                             params.get('project'),
                             params.get('pushlog_id'),
                             params.get('pushdate'),
                             self.time_interval,
                             self.push_interval):
            # Always optimize away low-value tasks
            return True
        else:
            return False


@register_strategy("skip-unless-changed")
class SkipUnlessChanged(OptimizationStrategy):
    def should_remove_task(self, task, params, file_patterns):
        # pushlog_id == -1 - this is the case when run from a cron.yml job
        if params.get('pushlog_id') == -1:
            return False

        changed = files_changed.check(params, file_patterns)
        if not changed:
            logger.debug('no files found matching a pattern in `skip-unless-changed` for ' +
                         task.label)
            return True
        return False


@register_strategy("skip-unless-schedules")
class SkipUnlessSchedules(OptimizationStrategy):

    @memoize
    def scheduled_by_push(self, repository, revision):
        changed_files = files_changed.get_changed_files(repository, revision)

        mbo = MozbuildObject.from_environment()
        # the decision task has a sparse checkout, so, mozbuild_reader will use
        # a MercurialRevisionFinder with revision '.', which should be the same
        # as `revision`; in other circumstances, it will use a default reader
        rdr = mbo.mozbuild_reader(config_mode='empty')

        components = set()
        for p, m in rdr.files_info(changed_files).items():
            components |= set(m['SCHEDULES'].components)

        return components

    def should_remove_task(self, task, params, conditions):
        if params.get('pushlog_id') == -1:
            return False

        scheduled = self.scheduled_by_push(params['head_repository'], params['head_rev'])
        conditions = set(conditions)
        # if *any* of the condition components are scheduled, do not optimize
        if conditions & scheduled:
            return False

        return True
