import logging
from datetime import datetime

from taskgraph import files_changed
from taskgraph.optimize.base import OptimizationStrategy, register_strategy
from taskgraph.util.taskcluster import find_task_id, status_task

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
        # pushlog_id == -1 - this is the case when run from a cron.yml job or on a git repository
        if params.get("repository_type") == "hg" and params.get("pushlog_id") == -1:
            return False

        changed = files_changed.check(params, file_patterns)
        if not changed:
            logger.debug(
                'no files found matching a pattern in `skip-unless-changed` for "{}"'.format(
                    task.label
                )
            )
            return True
        return False
