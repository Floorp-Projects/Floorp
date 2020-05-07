# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from collections import defaultdict

from six.moves.urllib.parse import urlsplit

from taskgraph.optimize import register_strategy, OptimizationStrategy, seta
from taskgraph.util.bugbug import (
    BugbugTimeoutException,
    push_schedules,
    CT_HIGH,
    CT_MEDIUM,
    CT_LOW,
)


@register_strategy("bugbug", args=(CT_MEDIUM,))
@register_strategy("bugbug-combined-high", args=(CT_HIGH, False, True))
@register_strategy("bugbug-low", args=(CT_LOW,))
@register_strategy("bugbug-high", args=(CT_HIGH,))
@register_strategy("bugbug-reduced", args=(CT_MEDIUM, True))
@register_strategy("bugbug-reduced-fallback", args=(CT_MEDIUM, True, False, True))
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

    def __init__(
        self,
        confidence_threshold,
        use_reduced_tasks=False,
        combine_weights=False,
        fallback=False,
    ):
        self.confidence_threshold = confidence_threshold
        self.use_reduced_tasks = use_reduced_tasks
        self.combine_weights = combine_weights
        self.fallback = fallback

    def should_remove_task(self, task, params, importance):
        branch = urlsplit(params['head_repository']).path.strip('/')
        rev = params['head_rev']
        try:
            data = push_schedules(branch, rev)
        except BugbugTimeoutException:
            if self.fallback:
                return seta.is_low_value_task(task.label, params['project'])

            raise

        key = "reduced_tasks" if self.use_reduced_tasks else "tasks"
        tasks = set(
            task
            for task, confidence in data.get(key, {}).items()
            if confidence >= self.confidence_threshold
        )

        test_manifests = task.attributes.get('test_manifests')
        if test_manifests is None or self.use_reduced_tasks:
            if "known_tasks" in data and task.label not in data["known_tasks"]:
                return False

            if task.label not in tasks:
                return True

            return False

        # If a task contains more than one group, figure out which confidence
        # threshold to use. If 'self.combine_weights' is set, add up all
        # confidence thresholds. Otherwise just use the max.
        task_confidence = 0
        groups = data.get("groups", {})
        for group, confidence in groups.items():
            if group not in test_manifests:
                continue

            if self.combine_weights:
                task_confidence = round(
                    task_confidence + confidence - task_confidence * confidence, 2
                )
            else:
                task_confidence = max(task_confidence, confidence)

        if task_confidence < self.confidence_threshold:
            return True

        # Store group importance so future optimizers can access it.
        for manifest in test_manifests:
            if manifest not in groups:
                continue

            confidence = groups[manifest]
            if confidence >= CT_HIGH:
                importance[manifest] = 'high'
            elif confidence >= CT_MEDIUM:
                importance[manifest] = 'medium'
            elif confidence >= CT_LOW:
                importance[manifest] = 'low'
            else:
                importance[manifest] = 'lowest'

        return False


@register_strategy("platform-debug")
class SkipUnlessDebug(OptimizationStrategy):
    """Only run debug platforms."""

    def should_remove_task(self, task, params, arg):
        return not (task.attributes.get('build_type') == "debug")


@register_strategy("platform-disperse")
class DisperseGroups(OptimizationStrategy):
    """Disperse groups across test configs.

    Each task has an associated 'importance' dict passed in via the arg. This
    is of the form `{<group>: <importance>}`.

    Where 'group' is a test group id (usually a path to a manifest), and 'importance' is
    one of `{'lowest', 'low', 'medium', 'high'}`.

    Each importance value has an associated 'count' as defined in
    `self.target_counts`. It guarantees that 'manifest' will run in at least
    'count' different configurations (assuming there are enough tasks
    containing 'manifest').

    On configurations that haven't been seen before, we'll increase the target
    count by `self.unseen_modifier` to increase the likelihood of scheduling a
    task on that configuration.

    Args:
        target_counts (dict): Override DEFAULT_TARGET_COUNTS with custom counts. This
            is a dict mapping the importance value ('lowest', 'low', etc) to the
            minimum number of configurations manifests with this value should run
            on.

        unseen_modifier (int): Override DEFAULT_UNSEEN_MODIFIER to a custom
            value. This is the amount we'll increase 'target_count' by for unseen
            configurations.
    """

    DEFAULT_TARGET_COUNTS = {
        'high': 3,
        'medium': 2,
        'low': 1,
        'lowest': 0,
    }
    DEFAULT_UNSEEN_MODIFIER = 1

    def __init__(self, target_counts=None, unseen_modifier=DEFAULT_UNSEEN_MODIFIER):
        self.target_counts = self.DEFAULT_TARGET_COUNTS
        if target_counts:
            self.target_counts.update(target_counts)
        self.unseen_modifier = unseen_modifier

        self.count = defaultdict(int)
        self.seen_configurations = set()

    def should_remove_task(self, task, params, importance):
        test_manifests = task.attributes.get("test_manifests")
        test_platform = task.attributes.get("test_platform")

        if not importance or not test_manifests or not test_platform:
            return False

        # Build the test configuration key.
        key = test_platform
        if 'unittest_variant' in task.attributes:
            key += "-" + task.attributes['unittest_variant']

        if not task.attributes["e10s"]:
            key += "-1proc"

        important_manifests = set(test_manifests) & set(importance)
        for manifest in important_manifests:
            target_count = self.target_counts[importance[manifest]]

            # If this configuration hasn't been seen before, increase the
            # likelihood of scheduling the task.
            if key not in self.seen_configurations:
                target_count += self.unseen_modifier

            if self.count[manifest] < target_count:
                # Update manifest counts and seen configurations.
                self.seen_configurations.add(key)
                for manifest in important_manifests:
                    self.count[manifest] += 1
                return False

        # Should remove task because all manifests have reached their
        # importance count (or there were no important manifests).
        return True
