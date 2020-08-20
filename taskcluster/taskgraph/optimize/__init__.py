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

from __future__ import absolute_import, print_function, unicode_literals

import logging
from abc import ABCMeta, abstractmethod, abstractproperty
from collections import defaultdict

import six
from slugid import nice as slugid

from taskgraph.graph import Graph
from taskgraph.taskgraph import TaskGraph
from taskgraph.util.parameterization import resolve_task_references
from taskgraph.util.python_path import import_sibling_modules

logger = logging.getLogger(__name__)
registry = {}


def register_strategy(name, args=()):
    def wrap(cls):
        if name not in registry:
            registry[name] = cls(*args)
            if not hasattr(registry[name], 'description'):
                registry[name].description = name
        return cls
    return wrap


def optimize_task_graph(target_task_graph, requested_tasks, params, do_not_optimize,
                        decision_task_id, existing_tasks=None, strategy_override=None):
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
        do_not_optimize=do_not_optimize)

    replaced_tasks = replace_tasks(
        target_task_graph=target_task_graph,
        optimizations=optimizations,
        params=params,
        do_not_optimize=do_not_optimize,
        label_to_taskid=label_to_taskid,
        existing_tasks=existing_tasks,
        removed_tasks=removed_tasks)

    return get_subgraph(
            target_task_graph, removed_tasks, replaced_tasks,
            label_to_taskid, decision_task_id), label_to_taskid


def _get_optimizations(target_task_graph, strategies):
    def optimizations(label):
        task = target_task_graph.tasks[label]
        if task.optimization:
            opt_by, arg = list(task.optimization.items())[0]
            strategy = strategies[opt_by]
            if hasattr(strategy, 'description'):
                opt_by += " ({})".format(strategy.description)
            return (opt_by, strategy, arg)
        else:
            return ('never', strategies['never'], None)
    return optimizations


def _log_optimization(verb, opt_counts, opt_reasons=None):
    if opt_reasons:
        message = "optimize: {label} {action} because of {reason}"
        for label, (action, reason) in opt_reasons.items():
            logger.debug(message.format(label=label, action=action, reason=reason))

    if opt_counts:
        logger.info(
            '{} '.format(verb.title()) +
            ', '.join(
                '{} tasks by {}'.format(c, b)
                for b, c in sorted(opt_counts.items())) +
            ' during optimization.')
    else:
        logger.info('No tasks {} during optimization'.format(verb))


def remove_tasks(target_task_graph, requested_tasks, params, optimizations, do_not_optimize):
    """
    Implement the "Removing Tasks" phase, returning a set of task labels of all removed tasks.
    """
    opt_counts = defaultdict(int)
    opt_reasons = {}
    removed = set()
    reverse_links_dict = target_task_graph.graph.reverse_links_dict()

    for label in target_task_graph.graph.visit_preorder():
        # if we're not allowed to optimize, that's easy..
        if label in do_not_optimize:
            opt_reasons[label] = ("kept", "do not optimize")
            continue

        # if there are remaining tasks depending on this one, do not remove..
        if any(l not in removed for l in reverse_links_dict[label]):
            opt_reasons[label] = ("kept", "dependent tasks")
            continue

        # Some tasks in the task graph only exist because they were required
        # by a task that has just been optimized away. They can now be removed.
        if label not in requested_tasks:
            opt_counts['dependents-optimized'] += 1
            opt_reasons[label] = ("removed", "dependents optimized")
            removed.add(label)

        # call the optimization strategy
        task = target_task_graph.tasks[label]
        opt_by, opt, arg = optimizations(label)
        if opt.should_remove_task(task, params, arg):
            opt_counts[opt_by] += 1
            opt_reasons[label] = ("removed", "'{}' strategy".format(opt_by))
            removed.add(label)

    _log_optimization('removed', opt_counts, opt_reasons)
    return removed


def replace_tasks(target_task_graph, params, optimizations, do_not_optimize,
                  label_to_taskid, removed_tasks, existing_tasks):
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
            opt_counts['existing_tasks'] += 1
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

    _log_optimization('replaced', opt_counts)
    return replaced


def get_subgraph(
    target_task_graph, removed_tasks, replaced_tasks, label_to_taskid, decision_task_id,
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
    bad_edges = [(l, r, n) for l, r, n in target_task_graph.graph.edges
                 if l not in removed_tasks and r in removed_tasks]
    if bad_edges:
        probs = ', '.join('{} depends on {} as {} but it has been removed'.format(l, r, n)
                          for l, r, n in bad_edges)
        raise Exception("Optimization error: " + probs)

    # fill in label_to_taskid for anything not removed or replaced
    assert replaced_tasks <= set(label_to_taskid)
    for label in sorted(target_task_graph.graph.nodes - removed_tasks - set(label_to_taskid)):
        label_to_taskid[label] = slugid().decode('ascii')

    # resolve labels to taskIds and populate task['dependencies']
    tasks_by_taskid = {}
    named_links_dict = target_task_graph.graph.named_links_dict()
    omit = removed_tasks | replaced_tasks
    for label, task in six.iteritems(target_task_graph.tasks):
        if label in omit:
            continue
        task.task_id = label_to_taskid[label]
        named_task_dependencies = {
            name: label_to_taskid[label]
            for name, label in named_links_dict.get(label, {}).items()}

        # Add remaining soft dependencies
        if task.soft_dependencies:
            named_task_dependencies.update({
                label: label_to_taskid[label]
                for label in task.soft_dependencies
                if label in label_to_taskid and label not in omit
            })

        task.task = resolve_task_references(
            task.label,
            task.task,
            task_id=task.task_id,
            decision_task_id=decision_task_id,
            dependencies=named_task_dependencies,
        )
        deps = task.task.setdefault('dependencies', [])
        deps.extend(sorted(named_task_dependencies.values()))
        tasks_by_taskid[task.task_id] = task

    # resolve edges to taskIds
    edges_by_taskid = (
        (label_to_taskid.get(left), label_to_taskid.get(right), name)
        for (left, right, name) in target_task_graph.graph.edges
    )
    # ..and drop edges that are no longer entirely in the task graph
    #   (note that this omits edges to replaced tasks, but they are still in task.dependnecies)
    edges_by_taskid = set(
        (left, right, name)
        for (left, right, name) in edges_by_taskid
        if left in tasks_by_taskid and right in tasks_by_taskid
    )

    return TaskGraph(
        tasks_by_taskid,
        Graph(set(tasks_by_taskid), edges_by_taskid))


@register_strategy('never')
class OptimizationStrategy(object):
    def should_remove_task(self, task, params, arg):
        """Determine whether to optimize this task by removing it.  Returns
        True to remove."""
        return False

    def should_replace_task(self, task, params, arg):
        """Determine whether to optimize this task by replacing it.  Returns a
        taskId to replace this task, True to replace with nothing, or False to
        keep the task."""
        return False


@register_strategy('always')
class Always(OptimizationStrategy):
    def should_remove_task(self, task, params, arg):
        return True


@six.add_metaclass(ABCMeta)
class CompositeStrategy(OptimizationStrategy):

    def __init__(self, *substrategies, **kwargs):
        self.substrategies = []
        missing = set()
        for sub in substrategies:
            if isinstance(sub, six.text_type):
                if sub not in registry.keys():
                    missing.add(sub)
                    continue
                sub = registry[sub]

            self.substrategies.append(sub)

        if missing:
            raise TypeError("substrategies aren't registered: {}".format(
                ",  ".join(sorted(missing))))

        self.split_args = kwargs.pop('split_args', None)
        if not self.split_args:
            self.split_args = lambda arg: [arg] * len(substrategies)
        if kwargs:
            raise TypeError("unexpected keyword args")

    @abstractproperty
    def description(self):
        """A textual description of the combined substrategies."""
        pass

    @abstractmethod
    def reduce(self, results):
        """Given all substrategy results as a generator, return the overall
        result."""
        pass

    def _generate_results(self, fname, task, params, arg):
        for sub, arg in zip(self.substrategies, self.split_args(arg)):
            yield getattr(sub, fname)(task, params, arg)

    def should_remove_task(self, *args):
        results = self._generate_results('should_remove_task', *args)
        return self.reduce(results)

    def should_replace_task(self, *args):
        results = self._generate_results('should_replace_task', *args)
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
        super(Alias, self).__init__(strategy)

    @property
    def description(self):
        return self.substrategies[0].description

    def reduce(self, results):
        return next(results)


def split_bugbug_arg(arg):
    """Split args for bugbug based strategies.

    Many bugbug based optimizations require passing an empty dict by reference
    to communicate to downstream strategies. This function passes the provided
    arg to the first strategy and an empty dict to second (bugbug based)
    strategy.
    """
    return (arg, {})


# Trigger registration in sibling modules.
import_sibling_modules()


# Register composite strategies.
register_strategy('build', args=('skip-unless-schedules',))(Alias)
register_strategy('build-fuzzing', args=('backstop-10-pushes-2-hours',))(Alias)
register_strategy('test', args=('skip-unless-schedules',))(Alias)
register_strategy('test-inclusive', args=('skip-unless-schedules',))(Alias)


# Strategy overrides used to tweak the default strategies. These are referenced
# by the `optimize_strategies` parameter.

class project(object):
    """Strategies that should be applied per-project."""

    # Optimize everything away, except on 20th pushes.
    register_strategy('full-backstop', args=('backstop-20-pushes-4-hours',))(Alias)

    # Optimize everything away, except on 10th pushes, where we run everything that was selected by
    # bugbug for the last 10 pushes.
    register_strategy(
        'optimized-backstop',
        args=(
            'backstop-10-pushes-2-hours',
            Any(
                'skip-unless-schedules',
                Any(
                    'bugbug-reduced-manifests-fallback-last-10-pushes',
                    'platform-disperse',
                ),
                split_args=split_bugbug_arg,
            ),
        ),
    )(Any)

    # The three strategies are part of an All composite strategy, which means they are linked
    # by AND.
    # - On 20th pushes, "full-backstop" will not allow the strategy to optimize anything away.
    # - On 10th pushes, "full-backstop" allows the strategy to optimize things away, but
    #   "optimized-backstop" will apply the relaxed bugbug optimization and will not allow the
    #   normal bugbug optimization to apply.
    # - On all other pushes, the normal bugbug optimization is applied.
    autoland = {
        'test': All(
            'full-backstop',
            'optimized-backstop',
            Any('skip-unless-schedules', 'bugbug-reduced-fallback', split_args=split_bugbug_arg),
        ),
        'build': All(
            'backstop-10-pushes-2-hours',
            Any(
                'skip-unless-schedules',
                'bugbug-reduced-fallback',
                split_args=split_bugbug_arg,
            ),
        ),
    }
    """Strategy overrides that apply to autoland."""


class experimental(object):
    """Experimental strategies either under development or used as benchmarks.

    These run as "shadow-schedulers" on each autoland push (tier 3) and/or can be used
    with `./mach try auto`.  E.g:

        ./mach try auto --strategy relevant_tests
    """

    bugbug_tasks_medium = {
        'test': Any('skip-unless-schedules', 'bugbug-tasks-medium', split_args=split_bugbug_arg),
    }
    """Doesn't limit platforms, medium confidence threshold."""

    bugbug_tasks_high = {
        'test': Any('skip-unless-schedules', 'bugbug-tasks-high', split_args=split_bugbug_arg),
    }
    """Doesn't limit platforms, high confidence threshold."""

    bugbug_debug_disperse = {
        'test': Any(
            'skip-unless-schedules',
            Any('bugbug-low', 'platform-debug', 'platform-disperse'),
            split_args=split_bugbug_arg
        ),
    }
    """Restricts tests to debug platforms."""

    bugbug_disperse_low = {
        'test': Any(
            'skip-unless-schedules',
            Any('bugbug-low', 'platform-disperse'),
            split_args=split_bugbug_arg
        ),
    }
    """Disperse tests across platforms, low confidence threshold."""

    bugbug_disperse_medium = {
        'test': Any(
            'skip-unless-schedules',
            Any('bugbug-medium', 'platform-disperse'),
            split_args=split_bugbug_arg
        ),
    }
    """Disperse tests across platforms, medium confidence threshold."""

    bugbug_disperse_reduced_medium = {
        'test': Any(
            'skip-unless-schedules',
            Any('bugbug-reduced-manifests', 'platform-disperse'),
            split_args=split_bugbug_arg
        ),
    }
    """Disperse tests across platforms, medium confidence threshold with reduced tasks."""

    bugbug_disperse_medium_no_unseen = {
        'test': Any(
            'skip-unless-schedules',
            Any('bugbug-medium', 'platform-disperse-no-unseen'),
            split_args=split_bugbug_arg
        ),
    }
    """Disperse tests across platforms (no modified for unseen configurations), medium confidence
    threshold."""

    bugbug_disperse_medium_only_one = {
        'test': Any(
            'skip-unless-schedules',
            Any('bugbug-medium', 'platform-disperse-only-one'),
            split_args=split_bugbug_arg
        ),
    }
    """Disperse tests across platforms (one platform per group), medium confidence threshold."""

    bugbug_disperse_high = {
        'test': Any(
            'skip-unless-schedules',
            Any('bugbug-high', 'platform-disperse'),
            split_args=split_bugbug_arg
        ),
    }
    """Disperse tests across platforms, high confidence threshold."""

    bugbug_reduced = {
        'test': Any('skip-unless-schedules', 'bugbug-reduced', split_args=split_bugbug_arg),
    }
    """Use the reduced set of tasks (and no groups) chosen by bugbug."""

    bugbug_reduced_high = {
        'test': Any('skip-unless-schedules', 'bugbug-reduced-high', split_args=split_bugbug_arg),
    }
    """Use the reduced set of tasks (and no groups) chosen by bugbug, high
    confidence threshold."""

    relevant_tests = {
        'test': Any('skip-unless-schedules', 'skip-unless-has-relevant-tests'),
    }
    """Runs task containing tests in the same directories as modified files."""


class ExperimentalOverride(object):
    """Overrides dictionaries that are stored in a container with new values.

    This can be used to modify all strategies in a collection the same way,
    presumably with strategies affecting kinds of tasks tangential to the
    current context.

    Args:
        base (object): A container class supporting attribute access.
        overrides (dict): Values to update any accessed dictionaries with.
    """
    def __init__(self, base, overrides):
        self.base = base
        self.overrides = overrides

    def __getattr__(self, name):
        val = getattr(self.base, name).copy()
        val.update(self.overrides)
        return val


tryselect = ExperimentalOverride(experimental, {
    'build': Any('skip-unless-schedules', 'bugbug-reduced', split_args=split_bugbug_arg),
    'build-fuzzing': Alias('bugbug-reduced'),
})
