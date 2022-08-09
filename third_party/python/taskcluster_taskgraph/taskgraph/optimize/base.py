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

import datetime
import logging
from abc import ABCMeta, abstractmethod, abstractproperty
from collections import defaultdict

from slugid import nice as slugid

from taskgraph.graph import Graph
from taskgraph.taskgraph import TaskGraph
from taskgraph.util.parameterization import resolve_task_references, resolve_timestamps
from taskgraph.util.python_path import import_sibling_modules

logger = logging.getLogger(__name__)
registry = {}


def register_strategy(name, args=()):
    def wrap(cls):
        if name not in registry:
            registry[name] = cls(*args)
            if not hasattr(registry[name], "description"):
                registry[name].description = name
        return cls

    return wrap


def optimize_task_graph(
    target_task_graph,
    requested_tasks,
    params,
    do_not_optimize,
    decision_task_id,
    existing_tasks=None,
    strategy_override=None,
):
    """
    Perform task optimization, returning a taskgraph and a map from label to
    assigned taskId, including replacement tasks.
    """
    label_to_taskid = {}
    if not existing_tasks:
        existing_tasks = {}

    # instantiate the strategies for this optimization process
    strategies = registry.copy()
    if strategy_override:
        strategies.update(strategy_override)

    optimizations = _get_optimizations(target_task_graph, strategies)

    removed_tasks = remove_tasks(
        target_task_graph=target_task_graph,
        requested_tasks=requested_tasks,
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


def _get_optimizations(target_task_graph, strategies):
    def optimizations(label):
        task = target_task_graph.tasks[label]
        if task.optimization:
            opt_by, arg = list(task.optimization.items())[0]
            strategy = strategies[opt_by]
            if hasattr(strategy, "description"):
                opt_by += f" ({strategy.description})"
            return (opt_by, strategy, arg)
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


def remove_tasks(
    target_task_graph, requested_tasks, params, optimizations, do_not_optimize
):
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

        # Some tasks in the task graph only exist because they were required
        # by a task that has just been optimized away. They can now be removed.
        if label not in requested_tasks:
            _remove("dependents optimized")
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
    dependents_of = target_task_graph.graph.reverse_links_dict()
    dependencies_of = target_task_graph.graph.links_dict()

    for label in target_task_graph.graph.visit_postorder():
        # if we're not allowed to optimize, that's easy..
        if label in do_not_optimize:
            continue

        # if this task depends on un-replaced, un-removed tasks, do not replace
        if any(
            l not in replaced and l not in removed_tasks for l in dependencies_of[label]
        ):
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

        # compute latest deadline of dependents (if any)
        dependents = [target_task_graph.tasks[l] for l in dependents_of[label]]
        deadline = None
        if dependents:
            now = datetime.datetime.utcnow()
            deadline = max(
                resolve_timestamps(now, task.task["deadline"]) for task in dependents
            )
        repl = opt.should_replace_task(task, params, deadline, arg)
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


@register_strategy("never")
class OptimizationStrategy:
    def should_remove_task(self, task, params, arg):
        """Determine whether to optimize this task by removing it.  Returns
        True to remove."""
        return False

    def should_replace_task(self, task, params, deadline, arg):
        """Determine whether to optimize this task by replacing it.  Returns a
        taskId to replace this task, True to replace with nothing, or False to
        keep the task."""
        return False


@register_strategy("always")
class Always(OptimizationStrategy):
    def should_remove_task(self, task, params, arg):
        return True


class CompositeStrategy(OptimizationStrategy, metaclass=ABCMeta):
    def __init__(self, *substrategies, **kwargs):
        self.substrategies = []
        missing = set()
        for sub in substrategies:
            if isinstance(sub, str):
                if sub not in registry.keys():
                    missing.add(sub)
                    continue
                sub = registry[sub]

            self.substrategies.append(sub)

        if missing:
            raise TypeError(
                "substrategies aren't registered: {}".format(
                    ",  ".join(sorted(missing))
                )
            )

        self.split_args = kwargs.pop("split_args", None)
        if not self.split_args:
            self.split_args = lambda arg, substrategies: [arg] * len(substrategies)
        if kwargs:
            raise TypeError("unexpected keyword args")

    @abstractproperty
    def description(self):
        """A textual description of the combined substrategies."""

    @abstractmethod
    def reduce(self, results):
        """Given all substrategy results as a generator, return the overall
        result."""

    def _generate_results(self, fname, *args):
        *passthru, arg = args
        for sub, arg in zip(
            self.substrategies, self.split_args(arg, self.substrategies)
        ):
            yield getattr(sub, fname)(*passthru, arg)

    def should_remove_task(self, *args):
        results = self._generate_results("should_remove_task", *args)
        return self.reduce(results)

    def should_replace_task(self, *args):
        results = self._generate_results("should_replace_task", *args)
        return self.reduce(results)


class Any(CompositeStrategy):
    """Given one or more optimization strategies, remove or replace a task if any of them
    says to.

    Replacement will use the value returned by the first strategy that says to replace.
    """

    @property
    def description(self):
        return "-or-".join([s.description for s in self.substrategies])

    @classmethod
    def reduce(cls, results):
        for rv in results:
            if rv:
                return rv
        return False


class All(CompositeStrategy):
    """Given one or more optimization strategies, remove or replace a task if all of them
    says to.

    Replacement will use the value returned by the first strategy passed in.
    Note the values used for replacement need not be the same, as long as they
    all say to replace.
    """

    @property
    def description(self):
        return "-and-".join([s.description for s in self.substrategies])

    @classmethod
    def reduce(cls, results):
        for rv in results:
            if not rv:
                return rv
        return True


class Alias(CompositeStrategy):
    """Provides an alias to an existing strategy.

    This can be useful to swap strategies in and out without needing to modify
    the task transforms.
    """

    def __init__(self, strategy):
        super().__init__(strategy)

    @property
    def description(self):
        return self.substrategies[0].description

    def reduce(self, results):
        return next(results)


class Not(CompositeStrategy):
    """Given a strategy, returns the opposite."""

    def __init__(self, strategy):
        super().__init__(strategy)

    @property
    def description(self):
        return "not-" + self.substrategies[0].description

    def reduce(self, results):
        return not next(results)


# Trigger registration in sibling modules.
import_sibling_modules()
