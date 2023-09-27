# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging

import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozbuild.util import memoize
from taskgraph.optimize.base import OptimizationStrategy, register_strategy

from gecko_taskgraph import files_changed

logger = logging.getLogger(__name__)


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
