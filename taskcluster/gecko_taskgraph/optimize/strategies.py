# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
from datetime import datetime

import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozbuild.util import memoize
from taskgraph.optimize.base import register_strategy, OptimizationStrategy
from taskgraph.util.taskcluster import find_task_id

from gecko_taskgraph import files_changed
from gecko_taskgraph.util.taskcluster import status_task

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

    fmt = "%Y-%m-%dT%H:%M:%S.%fZ"

    def should_replace_task(self, task, params, deadline, index_paths):
        "Look for a task with one of the given index paths"
        for index_path in index_paths:
            try:
                task_id = find_task_id(index_path)
                status = status_task(task_id)
                # status can be `None` if we're in `testing` mode
                # (e.g. test-action-callback)
                if not status or status.get("state") in ("exception", "failed"):
                    continue

                if deadline and datetime.strptime(
                    status["expires"], self.fmt
                ) < datetime.strptime(deadline, self.fmt):
                    continue

                return task_id
            except KeyError:
                # 404 will end up here and go on to the next index path
                pass

        return False


@register_strategy("skip-unless-changed")
class SkipUnlessChanged(OptimizationStrategy):
    def should_remove_task(self, task, params, file_patterns):
        # pushlog_id == -1 - this is the case when run from a cron.yml job
        if params.get("pushlog_id") == -1:
            return False

        changed = files_changed.check(params, file_patterns)
        if not changed:
            logger.debug(
                "no files found matching a pattern in `skip-unless-changed` for "
                + task.label
            )
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
        rdr = mbo.mozbuild_reader(config_mode="empty")

        components = set()
        for p, m in rdr.files_info(changed_files).items():
            components |= set(m["SCHEDULES"].components)

        return components

    def should_remove_task(self, task, params, conditions):
        if params.get("pushlog_id") == -1:
            return False

        scheduled = self.scheduled_by_push(
            params["head_repository"], params["head_rev"]
        )
        conditions = set(conditions)
        # if *any* of the condition components are scheduled, do not optimize
        if conditions & scheduled:
            return False

        return True


@register_strategy("skip-unless-has-relevant-tests")
class SkipUnlessHasRelevantTests(OptimizationStrategy):
    """Optimizes tasks that don't run any tests that were
    in child directories of a modified file.
    """

    @memoize
    def get_changed_dirs(self, repo, rev):
        changed = map(mozpath.dirname, files_changed.get_changed_files(repo, rev))
        # Filter out empty directories (from files modified in the root).
        # Otherwise all tasks would be scheduled.
        return {d for d in changed if d}

    def should_remove_task(self, task, params, _):
        if not task.attributes.get("test_manifests"):
            return True

        for d in self.get_changed_dirs(params["head_repository"], params["head_rev"]):
            for t in task.attributes["test_manifests"]:
                if t.startswith(d):
                    logger.debug(
                        "{} runs a test path ({}) contained by a modified file ({})".format(
                            task.label, t, d
                        )
                    )
                    return False
        return True
