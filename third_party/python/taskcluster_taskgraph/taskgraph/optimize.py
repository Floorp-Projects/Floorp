# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
The objective of optimization is to remove as many tasks from the graph as
possible, as efficiently as possible, thereby delivering useful results as
quickly as possible.  For example, ideally if only a test script is modified in
a push, then the resulting graph contains only the corresponding test suite
task.

See ``taskcluster/docs/optimization.rst`` for more information.
"""


import logging
import os
from collections import defaultdict

from slugid import nice as slugid

from . import files_changed
from .graph import Graph
from .taskgraph import TaskGraph
from .util.parameterization import resolve_task_references
from .util.taskcluster import find_task_id

logger = logging.getLogger(__name__)

TOPSRCDIR = os.path.abspath(os.path.join(__file__, "../../../"))


def optimize_task_graph(
    target_task_graph,
    params,
    do_not_optimize,
    decision_task_id,
    existing_tasks=None,
    strategies=None,
):
    """
    Perform task optimization, returning a taskgraph and a map from label to
    assigned taskId, including replacement tasks.
    """
    label_to_taskid = {}
    if not existing_tasks:
        existing_tasks = {}

    # instantiate the strategies for this optimization process
    if not strategies:
        strategies = _make_default_strategies()

    optimizations = _get_optimizations(target_task_graph, strategies)

    removed_tasks = remove_tasks(
        target_task_graph=target_task_graph,
        optimizations=optimizations,
        params=params,
        do_not_optimize=do_not_optimize,
    )

    replaced_tasks = replace_tasks(
        target_task_graph=target_task_graph,
        optimizations=optimizations,
        params=params,
        do_not_optimize=do_not_optimize,
        label_to_taskid=label_to_taskid,
        existing_tasks=existing_tasks,
        removed_tasks=removed_tasks,
    )

    return (
        get_subgraph(
            target_task_graph,
            removed_tasks,
            replaced_tasks,
            label_to_taskid,
            decision_task_id,
        ),
        label_to_taskid,
    )


def _make_default_strategies():
    return {
        "never": OptimizationStrategy(),  # "never" is the default behavior
        "index-search": IndexSearch(),
        "skip-unless-changed": SkipUnlessChanged(),
    }


def _get_optimizations(target_task_graph, strategies):
    def optimizations(label):
        task = target_task_graph.tasks[label]
        if task.optimization:
            opt_by, arg = list(task.optimization.items())[0]
            return (opt_by, strategies[opt_by], arg)
        else:
            return ("never", strategies["never"], None)

    return optimizations


def _log_optimization(verb, opt_counts, opt_reasons=None):
    if opt_reasons:
        message = "optimize: {label} {action} because of {reason}"
        for label, (action, reason) in opt_reasons.items():
            logger.debug(message.format(label=label, action=action, reason=reason))

    if opt_counts:
        logger.info(
            f"{verb.title()} "
            + ", ".join(f"{c} tasks by {b}" for b, c in sorted(opt_counts.items()))
            + " during optimization."
        )
    else:
        logger.info(f"No tasks {verb} during optimization")


def remove_tasks(target_task_graph, params, optimizations, do_not_optimize):
    """
    Implement the "Removing Tasks" phase, returning a set of task labels of all removed tasks.
    """
    opt_counts = defaultdict(int)
    opt_reasons = {}
    removed = set()
    dependents_of = target_task_graph.graph.reverse_links_dict()
    tasks = target_task_graph.tasks
    prune_candidates = set()

    # Traverse graph so dependents (child nodes) are guaranteed to be processed
    # first.
    for label in target_task_graph.graph.visit_preorder():
        # Dependents that can be pruned away (shouldn't cause this task to run).
        # Only dependents that either:
        #   A) Explicitly reference this task in their 'if_dependencies' list, or
        #   B) Don't have an 'if_dependencies' attribute (i.e are in 'prune_candidates'
        #      because they should be removed but have prune_deps themselves)
        # should be considered.
        prune_deps = {
            l
            for l in dependents_of[label]
            if l in prune_candidates
            if not tasks[l].if_dependencies or label in tasks[l].if_dependencies
        }

        def _keep(reason):
            """Mark a task as being kept in the graph. Also recursively removes
            any dependents from `prune_candidates`, assuming they should be
            kept because of this task.
            """
            opt_reasons[label] = ("kept", reason)

            # Removes dependents that were in 'prune_candidates' from a task
            # that ended up being kept (and therefore the dependents should
            # also be kept).
            queue = list(prune_deps)
            while queue:
                l = queue.pop()

                # If l is a prune_dep of multiple tasks it could be queued up
                # multiple times. Guard against it being already removed.
                if l not in prune_candidates:
                    continue

                # If a task doesn't set 'if_dependencies' itself (rather it was
                # added to 'prune_candidates' due to one of its depenendents),
                # then we shouldn't remove it.
                if not tasks[l].if_dependencies:
                    continue

                prune_candidates.remove(l)
                queue.extend([r for r in dependents_of[l] if r in prune_candidates])

        def _remove(reason):
            """Potentially mark a task as being removed from the graph. If the
            task has dependents that can be pruned, add this task to
            `prune_candidates` rather than removing it.
            """
            if prune_deps:
                # If there are prune_deps, unsure if we can remove this task yet.
                prune_candidates.add(label)
            else:
                opt_reasons[label] = ("removed", reason)
                opt_counts[reason] += 1
                removed.add(label)

        # if we're not allowed to optimize, that's easy..
        if label in do_not_optimize:
            _keep("do not optimize")
            continue

        # If there are remaining tasks depending on this one, do not remove.
        if any(
            l for l in dependents_of[label] if l not in removed and l not in prune_deps
        ):
            _keep("dependent tasks")
            continue

        # Call the optimization strategy.
        task = tasks[label]
        opt_by, opt, arg = optimizations(label)
        if opt.should_remove_task(task, params, arg):
            _remove(opt_by)
            continue

        # Some tasks should only run if their dependency was also run. Since we
        # haven't processed dependencies yet, we add them to a list of
        # candidate tasks for pruning.
        if task.if_dependencies:
            opt_reasons[label] = ("kept", opt_by)
            prune_candidates.add(label)
        else:
            _keep(opt_by)

    if prune_candidates:
        reason = "if-dependencies pruning"
        for label in prune_candidates:
            # There's an edge case where a triangle graph can cause a
            # dependency to stay in 'prune_candidates' when the dependent
            # remains. Do a final check to ensure we don't create any bad
            # edges.
            dependents = any(
                d
                for d in dependents_of[label]
                if d not in prune_candidates
                if d not in removed
            )
            if dependents:
                opt_reasons[label] = ("kept", "dependent tasks")
                continue
            removed.add(label)
            opt_counts[reason] += 1
            opt_reasons[label] = ("removed", reason)

    _log_optimization("removed", opt_counts, opt_reasons)
    return removed


def replace_tasks(
    target_task_graph,
    params,
    optimizations,
    do_not_optimize,
    label_to_taskid,
    removed_tasks,
    existing_tasks,
):
    """
    Implement the "Replacing Tasks" phase, returning a set of task labels of
    all replaced tasks. The replacement taskIds are added to label_to_taskid as
    a side-effect.
    """
    opt_counts = defaultdict(int)
    replaced = set()
    links_dict = target_task_graph.graph.links_dict()

    for label in target_task_graph.graph.visit_postorder():
        # if we're not allowed to optimize, that's easy..
        if label in do_not_optimize:
            continue

        # if this task depends on un-replaced, un-removed tasks, do not replace
        if any(l not in replaced and l not in removed_tasks for l in links_dict[label]):
            continue

        # if the task already exists, that's an easy replacement
        repl = existing_tasks.get(label)
        if repl:
            label_to_taskid[label] = repl
            replaced.add(label)
            opt_counts["existing_tasks"] += 1
            continue

        # call the optimization strategy
        task = target_task_graph.tasks[label]
        opt_by, opt, arg = optimizations(label)
        repl = opt.should_replace_task(task, params, arg)
        if repl:
            if repl is True:
                # True means remove this task; get_subgraph will catch any
                # problems with removed tasks being depended on
                removed_tasks.add(label)
            else:
                label_to_taskid[label] = repl
                replaced.add(label)
            opt_counts[opt_by] += 1
            continue

    _log_optimization("replaced", opt_counts)
    return replaced


def get_subgraph(
    target_task_graph,
    removed_tasks,
    replaced_tasks,
    label_to_taskid,
    decision_task_id,
):
    """
    Return the subgraph of target_task_graph consisting only of
    non-optimized tasks and edges between them.

    To avoid losing track of taskIds for tasks optimized away, this method
    simultaneously substitutes real taskIds for task labels in the graph, and
    populates each task definition's `dependencies` key with the appropriate
    taskIds.  Task references are resolved in the process.
    """

    # check for any dependency edges from included to removed tasks
    bad_edges = [
        (l, r, n)
        for l, r, n in target_task_graph.graph.edges
        if l not in removed_tasks and r in removed_tasks
    ]
    if bad_edges:
        probs = ", ".join(
            f"{l} depends on {r} as {n} but it has been removed"
            for l, r, n in bad_edges
        )
        raise Exception("Optimization error: " + probs)

    # fill in label_to_taskid for anything not removed or replaced
    assert replaced_tasks <= set(label_to_taskid)
    for label in sorted(
        target_task_graph.graph.nodes - removed_tasks - set(label_to_taskid)
    ):
        label_to_taskid[label] = slugid()

    # resolve labels to taskIds and populate task['dependencies']
    tasks_by_taskid = {}
    named_links_dict = target_task_graph.graph.named_links_dict()
    omit = removed_tasks | replaced_tasks
    for label, task in target_task_graph.tasks.items():
        if label in omit:
            continue
        task.task_id = label_to_taskid[label]
        named_task_dependencies = {
            name: label_to_taskid[label]
            for name, label in named_links_dict.get(label, {}).items()
        }

        # Add remaining soft dependencies
        if task.soft_dependencies:
            named_task_dependencies.update(
                {
                    label: label_to_taskid[label]
                    for label in task.soft_dependencies
                    if label in label_to_taskid and label not in omit
                }
            )

        task.task = resolve_task_references(
            task.label,
            task.task,
            task_id=task.task_id,
            decision_task_id=decision_task_id,
            dependencies=named_task_dependencies,
        )
        deps = task.task.setdefault("dependencies", [])
        deps.extend(sorted(named_task_dependencies.values()))
        tasks_by_taskid[task.task_id] = task

    # resolve edges to taskIds
    edges_by_taskid = (
        (label_to_taskid.get(left), label_to_taskid.get(right), name)
        for (left, right, name) in target_task_graph.graph.edges
    )
    # ..and drop edges that are no longer entirely in the task graph
    #   (note that this omits edges to replaced tasks, but they are still in task.dependnecies)
    edges_by_taskid = {
        (left, right, name)
        for (left, right, name) in edges_by_taskid
        if left in tasks_by_taskid and right in tasks_by_taskid
    }

    return TaskGraph(tasks_by_taskid, Graph(set(tasks_by_taskid), edges_by_taskid))


class OptimizationStrategy:
    def should_remove_task(self, task, params, arg):
        """Determine whether to optimize this task by removing it.  Returns
        True to remove."""
        return False

    def should_replace_task(self, task, params, arg):
        """Determine whether to optimize this task by replacing it.  Returns a
        taskId to replace this task, True to replace with nothing, or False to
        keep the task."""
        return False


class Either(OptimizationStrategy):
    """Given one or more optimization strategies, remove a task if any of them
    says to, and replace with a task if any finds a replacement (preferring the
    earliest).  By default, each substrategy gets the same arg, but split_args
    can return a list of args for each strategy, if desired."""

    def __init__(self, *substrategies, **kwargs):
        self.substrategies = substrategies
        self.split_args = kwargs.pop("split_args", None)
        if not self.split_args:
            self.split_args = lambda arg: [arg] * len(substrategies)
        if kwargs:
            raise TypeError("unexpected keyword args")

    def _for_substrategies(self, arg, fn):
        for sub, arg in zip(self.substrategies, self.split_args(arg)):
            rv = fn(sub, arg)
            if rv:
                return rv
        return False

    def should_remove_task(self, task, params, arg):
        return self._for_substrategies(
            arg, lambda sub, arg: sub.should_remove_task(task, params, arg)
        )

    def should_replace_task(self, task, params, arg):
        return self._for_substrategies(
            arg, lambda sub, arg: sub.should_replace_task(task, params, arg)
        )


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
                    index_path, use_proxy=bool(os.environ.get("TASK_ID"))
                )
                return task_id
            except KeyError:
                # 404 will end up here and go on to the next index path
                pass

        return False


class SkipUnlessChanged(OptimizationStrategy):
    def should_remove_task(self, task, params, file_patterns):
        if params.get("repository_type") != "hg":
            raise RuntimeError(
                "SkipUnlessChanged optimization only works with mercurial repositories"
            )

        # pushlog_id == -1 - this is the case when run from a cron.yml job
        if params.get("pushlog_id") == -1:
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
