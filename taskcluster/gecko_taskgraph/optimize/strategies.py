# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging

import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozbuild.util import memoize
from taskgraph.optimize.base import OptimizationStrategy, register_strategy
from taskgraph.util.path import match as match_path

logger = logging.getLogger(__name__)


@register_strategy("skip-unless-schedules")
class SkipUnlessSchedules(OptimizationStrategy):
    @memoize
    def scheduled_by_push(self, files_changed):
        mbo = MozbuildObject.from_environment()
        # the decision task has a sparse checkout, so, mozbuild_reader will use
        # a MercurialRevisionFinder with revision '.', which should be the same
        # as `revision`; in other circumstances, it will use a default reader
        rdr = mbo.mozbuild_reader(config_mode="empty")

        components = set()
        for p, m in rdr.files_info(files_changed).items():
            components |= set(m["SCHEDULES"].components)

        return components

    def should_remove_task(self, task, params, conditions):
        if params.get("pushlog_id") == -1:
            return False

        scheduled = self.scheduled_by_push(frozenset(params["files_changed"]))
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
    def get_changed_dirs(self, files_changed):
        changed = map(mozpath.dirname, files_changed)
        # Filter out empty directories (from files modified in the root).
        # Otherwise all tasks would be scheduled.
        return {d for d in changed if d}

    def should_remove_task(self, task, params, _):
        if not task.attributes.get("test_manifests"):
            return True

        for d in self.get_changed_dirs(frozenset(params["files_changed"])):
            for t in task.attributes["test_manifests"]:
                if t.startswith(d):
                    logger.debug(
                        "{} runs a test path ({}) contained by a modified file ({})".format(
                            task.label, t, d
                        )
                    )
                    return False
        return True


# TODO: This overwrites upstream Taskgraph's `skip-unless-changed`
# optimization. Once the firefox-android migration is landed and we upgrade
# upstream Taskgraph to a version that doesn't call files_changed.check`, this
# class can be deleted. Also remove the `taskgraph.optimize.base.registry` tweak
# in `gecko_taskgraph.register` at the same time.
@register_strategy("skip-unless-changed")
class SkipUnlessChanged(OptimizationStrategy):
    def check(self, files_changed, patterns):
        for pattern in patterns:
            for path in files_changed:
                if match_path(path, pattern):
                    return True
        return False

    def should_remove_task(self, task, params, file_patterns):
        # pushlog_id == -1 - this is the case when run from a cron.yml job or on a git repository
        if params.get("repository_type") == "hg" and params.get("pushlog_id") == -1:
            return False

        changed = self.check(params["files_changed"], file_patterns)
        if not changed:
            logger.debug(
                'no files found matching a pattern in `skip-unless-changed` for "{}"'.format(
                    task.label
                )
            )
            return True
        return False
