# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from taskgraph import graph, optimize
from taskgraph.optimize import OptimizationStrategy
from taskgraph.taskgraph import TaskGraph
from taskgraph.task import Task
from mozunit import main
from slugid import nice as slugid


class Remove(OptimizationStrategy):

    def should_remove_task(self, task, params, arg):
        return True


class Replace(OptimizationStrategy):

    def should_replace_task(self, task, params, taskid):
        return taskid


class TestOptimize(unittest.TestCase):

    strategies = {
        'never': OptimizationStrategy(),
        'remove': Remove(),
        'replace': Replace(),
    }

    def make_task(self, label, optimization=None, task_def=None, optimized=None,
                  task_id=None, dependencies=None):
        task_def = task_def or {'sample': 'task-def'}
        task = Task(kind='test', label=label, attributes={}, task=task_def)
        task.optimization = optimization
        task.task_id = task_id
        if dependencies is not None:
            task.task['dependencies'] = sorted(dependencies)
        return task

    def make_graph(self, *tasks_and_edges):
        tasks = {t.label: t for t in tasks_and_edges if isinstance(t, Task)}
        edges = {e for e in tasks_and_edges if not isinstance(e, Task)}
        return TaskGraph(tasks, graph.Graph(set(tasks), edges))

    def make_opt_graph(self, *tasks_and_edges):
        tasks = {t.task_id: t for t in tasks_and_edges if isinstance(t, Task)}
        edges = {e for e in tasks_and_edges if not isinstance(e, Task)}
        return TaskGraph(tasks, graph.Graph(set(tasks), edges))

    def make_triangle(self, **opts):
        """
        Make a "triangle" graph like this:

          t1 <-------- t3
           `---- t2 --'
        """
        return self.make_graph(
            self.make_task('t1', opts.get('t1')),
            self.make_task('t2', opts.get('t2')),
            self.make_task('t3', opts.get('t3')),
            ('t3', 't2', 'dep'),
            ('t3', 't1', 'dep2'),
            ('t2', 't1', 'dep'))

    def assert_remove_tasks(self, graph, exp_removed, do_not_optimize=set()):
        got_removed = optimize.remove_tasks(
            target_task_graph=graph,
            optimizations=optimize._get_optimizations(graph, self.strategies),
            params={},
            do_not_optimize=do_not_optimize)
        self.assertEqual(got_removed, exp_removed)

    def test_remove_tasks_never(self):
        "A graph full of optimization=never has nothing removed"
        graph = self.make_triangle()
        self.assert_remove_tasks(graph, set())

    def test_remove_tasks_all(self):
        "A graph full of optimization=remove has removes everything"
        graph = self.make_triangle(
            t1={'remove': None},
            t2={'remove': None},
            t3={'remove': None})
        self.assert_remove_tasks(graph, {'t1', 't2', 't3'})

    def test_remove_tasks_blocked(self):
        "Removable tasks that are depended on by non-removable tasks are not removed"
        graph = self.make_triangle(
            t1={'remove': None},
            t3={'remove': None})
        self.assert_remove_tasks(graph, {'t3'})

    def test_remove_tasks_do_not_optimize(self):
        "Removable tasks that are marked do_not_optimize are not removed"
        graph = self.make_triangle(
            t1={'remove': None},
            t2={'remove': None},  # but do_not_optimize
            t3={'remove': None})
        self.assert_remove_tasks(graph, {'t3'}, do_not_optimize={'t2'})

    def assert_replace_tasks(self, graph, exp_replaced, exp_removed=set(), exp_label_to_taskid={},
                             do_not_optimize=None, label_to_taskid=None, removed_tasks=None,
                             existing_tasks=None):
        do_not_optimize = do_not_optimize or set()
        label_to_taskid = label_to_taskid or {}
        removed_tasks = removed_tasks or set()
        existing_tasks = existing_tasks or {}

        got_replaced = optimize.replace_tasks(
            target_task_graph=graph,
            optimizations=optimize._get_optimizations(graph, self.strategies),
            params={},
            do_not_optimize=do_not_optimize,
            label_to_taskid=label_to_taskid,
            removed_tasks=removed_tasks,
            existing_tasks=existing_tasks)
        self.assertEqual(got_replaced, exp_replaced)
        self.assertEqual(removed_tasks, exp_removed)
        self.assertEqual(label_to_taskid, exp_label_to_taskid)

    def test_replace_tasks_never(self):
        "No tasks are replaced when strategy is 'never'"
        graph = self.make_triangle()
        self.assert_replace_tasks(graph, set())

    def test_replace_tasks_all(self):
        "All replacable tasks are replaced when strategy is 'replace'"
        graph = self.make_triangle(
            t1={'replace': 'e1'},
            t2={'replace': 'e2'},
            t3={'replace': 'e3'})
        self.assert_replace_tasks(
            graph,
            exp_replaced={'t1', 't2', 't3'},
            exp_label_to_taskid={'t1': 'e1', 't2': 'e2', 't3': 'e3'})

    def test_replace_tasks_blocked(self):
        "A task cannot be replaced if it depends on one that was not replaced"
        graph = self.make_triangle(
            t1={'replace': 'e1'},
            t3={'replace': 'e3'})
        self.assert_replace_tasks(
            graph,
            exp_replaced={'t1'},
            exp_label_to_taskid={'t1': 'e1'})

    def test_replace_tasks_do_not_optimize(self):
        "A task cannot be replaced if it depends on one that was not replaced"
        graph = self.make_triangle(
            t1={'replace': 'e1'},
            t2={'replace': 'xxx'},  # but do_not_optimize
            t3={'replace': 'e3'})
        self.assert_replace_tasks(
            graph,
            exp_replaced={'t1'},
            exp_label_to_taskid={'t1': 'e1'},
            do_not_optimize={'t2'})

    def test_replace_tasks_removed(self):
        "A task can be replaced with nothing"
        graph = self.make_triangle(
            t1={'replace': 'e1'},
            t2={'replace': True},
            t3={'replace': True})
        self.assert_replace_tasks(
            graph,
            exp_replaced={'t1'},
            exp_removed={'t2', 't3'},
            exp_label_to_taskid={'t1': 'e1'})

    def assert_subgraph(self, graph, removed_tasks, replaced_tasks,
                        label_to_taskid, exp_subgraph, exp_label_to_taskid):
        self.maxDiff = None
        optimize.slugid = ('tid{}'.format(i) for i in xrange(1, 10)).next
        try:
            got_subgraph = optimize.get_subgraph(graph, removed_tasks,
                                                 replaced_tasks, label_to_taskid)
        finally:
            optimize.slugid = slugid
        self.assertEqual(got_subgraph.graph, exp_subgraph.graph)
        self.assertEqual(got_subgraph.tasks, exp_subgraph.tasks)
        self.assertEqual(label_to_taskid, exp_label_to_taskid)

    def test_get_subgraph_no_change(self):
        "get_subgraph returns a similarly-shaped subgraph when nothing is removed"
        graph = self.make_triangle()
        self.assert_subgraph(
            graph, set(), set(), {},
            self.make_opt_graph(
                self.make_task('t1', task_id='tid1', dependencies={}),
                self.make_task('t2', task_id='tid2', dependencies={'tid1'}),
                self.make_task('t3', task_id='tid3', dependencies={'tid1', 'tid2'}),
                ('tid3', 'tid2', 'dep'),
                ('tid3', 'tid1', 'dep2'),
                ('tid2', 'tid1', 'dep')),
            {'t1': 'tid1', 't2': 'tid2', 't3': 'tid3'})

    def test_get_subgraph_removed(self):
        "get_subgraph returns a smaller subgraph when tasks are removed"
        graph = self.make_triangle()
        self.assert_subgraph(
            graph, {'t2', 't3'}, set(), {},
            self.make_opt_graph(
                self.make_task('t1', task_id='tid1', dependencies={})),
            {'t1': 'tid1'})

    def test_get_subgraph_replaced(self):
        "get_subgraph returns a smaller subgraph when tasks are replaced"
        graph = self.make_triangle()
        self.assert_subgraph(
            graph, set(), {'t1', 't2'}, {'t1': 'e1', 't2': 'e2'},
            self.make_opt_graph(
                self.make_task('t3', task_id='tid1', dependencies={'e1', 'e2'})),
            {'t1': 'e1', 't2': 'e2', 't3': 'tid1'})

    def test_get_subgraph_removed_dep(self):
        "get_subgraph raises an Exception when a task depends on a removed task"
        graph = self.make_triangle()
        with self.assertRaises(Exception):
            optimize.get_subgraph(graph, {'t2'}, set(), {})


if __name__ == '__main__':
    main()
