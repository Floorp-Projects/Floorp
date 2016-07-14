# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..optimize import optimize_task_graph, resolve_task_references
from ..optimize import annotate_task_graph, get_subgraph
from ..taskgraph import TaskGraph
from .. import graph
from .util import TestTask


class TestResolveTaskReferences(unittest.TestCase):

    def do(self, input, output):
        taskid_for_edge_name = {'edge%d' % n: 'tid%d' % n for n in range(1, 4)}
        self.assertEqual(resolve_task_references('subject', input, taskid_for_edge_name), output)

    def test_in_list(self):
        "resolve_task_references resolves task references in a list"
        self.do({'in-a-list': ['stuff', {'task-reference': '<edge1>'}]},
                {'in-a-list': ['stuff', 'tid1']})

    def test_in_dict(self):
        "resolve_task_references resolves task references in a dict"
        self.do({'in-a-dict': {'stuff': {'task-reference': '<edge2>'}}},
                {'in-a-dict': {'stuff': 'tid2'}})

    def test_multiple(self):
        "resolve_task_references resolves multiple references in the same string"
        self.do({'multiple': {'task-reference': 'stuff <edge1> stuff <edge2> after'}},
                {'multiple': 'stuff tid1 stuff tid2 after'})

    def test_embedded(self):
        "resolve_task_references resolves ebmedded references"
        self.do({'embedded': {'task-reference': 'stuff before <edge3> stuff after'}},
                {'embedded': 'stuff before tid3 stuff after'})

    def test_escaping(self):
        "resolve_task_references resolves escapes in task references"
        self.do({'escape': {'task-reference': '<<><edge3>>'}},
                {'escape': '<tid3>'})

    def test_invalid(self):
        "resolve_task_references raises a KeyError on reference to an invalid task"
        self.assertRaisesRegexp(
            KeyError,
            "task 'subject' has no dependency with label 'no-such'",
            lambda: resolve_task_references('subject', {'task-reference': '<no-such>'}, {})
        )


class OptimizingTask(TestTask):
    # the `optimize` method on this class is overridden direclty in the tests
    # below.
    pass


class TestOptimize(unittest.TestCase):

    kind = None

    def make_task(self, label, task_def=None, optimized=None, task_id=None):
        task_def = task_def or {'sample': 'task-def'}
        task = OptimizingTask(label=label, task=task_def)
        task.optimized = optimized
        task.task_id = task_id
        return task

    def make_graph(self, *tasks_and_edges):
        tasks = {t.label: t for t in tasks_and_edges if isinstance(t, OptimizingTask)}
        edges = {e for e in tasks_and_edges if not isinstance(e, OptimizingTask)}
        return TaskGraph(tasks, graph.Graph(set(tasks), edges))

    def assert_annotations(self, graph, **annotations):
        def repl(task_id):
            return 'SLUGID' if task_id and len(task_id) == 22 else task_id
        got_annotations = {
            t.label: (t.optimized, repl(t.task_id)) for t in graph.tasks.itervalues()
        }
        self.assertEqual(got_annotations, annotations)

    def test_annotate_task_graph_no_optimize(self):
        "annotating marks everything as un-optimized if the kind returns that"
        OptimizingTask.optimize = lambda self: (False, None)
        graph = self.make_graph(
            self.make_task('task1'),
            self.make_task('task2'),
            self.make_task('task3'),
            ('task2', 'task1', 'build'),
            ('task2', 'task3', 'image'),
        )
        annotate_task_graph(graph, set(), graph.graph.named_links_dict(), {}, None)
        self.assert_annotations(
            graph,
            task1=(False, None),
            task2=(False, None),
            task3=(False, None)
        )

    def test_annotate_task_graph_taskid_without_optimize(self):
        "raises exception if kind returns a taskid without optimizing"
        OptimizingTask.optimize = lambda self: (False, 'some-taskid')
        graph = self.make_graph(self.make_task('task1'))
        self.assertRaises(
            Exception,
            lambda: annotate_task_graph(graph, set(), graph.graph.named_links_dict(), {}, None)
        )

    def test_annotate_task_graph_optimize_away_dependency(self):
        "raises exception if kind optimizes away a task on which another depends"
        OptimizingTask.optimize = \
            lambda self: (True, None) if self.label == 'task1' else (False, None)
        graph = self.make_graph(
            self.make_task('task1'),
            self.make_task('task2'),
            ('task2', 'task1', 'build'),
        )
        self.assertRaises(
            Exception,
            lambda: annotate_task_graph(graph, set(), graph.graph.named_links_dict(), {}, None)
        )

    def test_annotate_task_graph_do_not_optimize(self):
        "annotating marks everything as un-optimized if in do_not_optimize"
        OptimizingTask.optimize = lambda self: (True, 'taskid')
        graph = self.make_graph(
            self.make_task('task1'),
            self.make_task('task2'),
            ('task2', 'task1', 'build'),
        )
        label_to_taskid = {}
        annotate_task_graph(graph, {'task1', 'task2'},
                            graph.graph.named_links_dict(), label_to_taskid, None)
        self.assert_annotations(
            graph,
            task1=(False, None),
            task2=(False, None)
        )
        self.assertEqual

    def test_annotate_task_graph_nos_propagate(self):
        "annotating marks a task with a non-optimized dependency as non-optimized"
        OptimizingTask.optimize = \
            lambda self: (False, None) if self.label == 'task1' else (True, 'taskid')
        graph = self.make_graph(
            self.make_task('task1'),
            self.make_task('task2'),
            self.make_task('task3'),
            ('task2', 'task1', 'build'),
            ('task2', 'task3', 'image'),
        )
        annotate_task_graph(graph, set(),
                            graph.graph.named_links_dict(), {}, None)
        self.assert_annotations(
            graph,
            task1=(False, None),
            task2=(False, None),  # kind would have returned (True, 'taskid') here
            task3=(True, 'taskid')
        )

    def test_get_subgraph_single_dep(self):
        "when a single dependency is optimized, it is omitted from the graph"
        graph = self.make_graph(
            self.make_task('task1', optimized=True, task_id='dep1'),
            self.make_task('task2', optimized=False),
            self.make_task('task3', optimized=False),
            ('task2', 'task1', 'build'),
            ('task2', 'task3', 'image'),
        )
        label_to_taskid = {'task1': 'dep1'}
        sub = get_subgraph(graph, graph.graph.named_links_dict(), label_to_taskid)
        task2 = label_to_taskid['task2']
        task3 = label_to_taskid['task3']
        self.assertEqual(sub.graph.nodes, {task2, task3})
        self.assertEqual(sub.graph.edges, {(task2, task3, 'image')})
        self.assertEqual(sub.tasks[task2].task_id, task2)
        self.assertEqual(sorted(sub.tasks[task2].task['dependencies']),
                         sorted([task3, 'dep1']))
        self.assertEqual(sub.tasks[task3].task_id, task3)
        self.assertEqual(sorted(sub.tasks[task3].task['dependencies']), [])

    def test_get_subgraph_dep_chain(self):
        "when a dependency chain is optimized, it is omitted from the graph"
        graph = self.make_graph(
            self.make_task('task1', optimized=True, task_id='dep1'),
            self.make_task('task2', optimized=True, task_id='dep2'),
            self.make_task('task3', optimized=False),
            ('task2', 'task1', 'build'),
            ('task3', 'task2', 'image'),
        )
        label_to_taskid = {'task1': 'dep1', 'task2': 'dep2'}
        sub = get_subgraph(graph, graph.graph.named_links_dict(), label_to_taskid)
        task3 = label_to_taskid['task3']
        self.assertEqual(sub.graph.nodes, {task3})
        self.assertEqual(sub.graph.edges, set())
        self.assertEqual(sub.tasks[task3].task_id, task3)
        self.assertEqual(sorted(sub.tasks[task3].task['dependencies']), ['dep2'])

    def test_get_subgraph_opt_away(self):
        "when a leaf task is optimized away, it is omitted from the graph"
        graph = self.make_graph(
            self.make_task('task1', optimized=False),
            self.make_task('task2', optimized=True),
            ('task2', 'task1', 'build'),
        )
        label_to_taskid = {'task2': 'dep2'}
        sub = get_subgraph(graph, graph.graph.named_links_dict(), label_to_taskid)
        task1 = label_to_taskid['task1']
        self.assertEqual(sub.graph.nodes, {task1})
        self.assertEqual(sub.graph.edges, set())
        self.assertEqual(sub.tasks[task1].task_id, task1)
        self.assertEqual(sorted(sub.tasks[task1].task['dependencies']), [])

    def test_get_subgraph_refs_resolved(self):
        "get_subgraph resolves task references"
        graph = self.make_graph(
            self.make_task('task1', optimized=True, task_id='dep1'),
            self.make_task(
                'task2',
                optimized=False,
                task_def={'payload': {'task-reference': 'http://<build>/<test>'}}
            ),
            ('task2', 'task1', 'build'),
            ('task2', 'task3', 'test'),
            self.make_task('task3', optimized=False),
        )
        label_to_taskid = {'task1': 'dep1'}
        sub = get_subgraph(graph, graph.graph.named_links_dict(), label_to_taskid)
        task2 = label_to_taskid['task2']
        task3 = label_to_taskid['task3']
        self.assertEqual(sub.graph.nodes, {task2, task3})
        self.assertEqual(sub.graph.edges, {(task2, task3, 'test')})
        self.assertEqual(sub.tasks[task2].task_id, task2)
        self.assertEqual(sorted(sub.tasks[task2].task['dependencies']), sorted([task3, 'dep1']))
        self.assertEqual(sub.tasks[task2].task['payload'], 'http://dep1/' + task3)
        self.assertEqual(sub.tasks[task3].task_id, task3)

    def test_optimize(self):
        "optimize_task_graph annotates and extracts the subgraph from a simple graph"
        OptimizingTask.optimize = \
            lambda self: (True, 'dep1') if self.label == 'task1' else (False, None)
        input = self.make_graph(
            self.make_task('task1'),
            self.make_task('task2'),
            self.make_task('task3'),
            ('task2', 'task1', 'build'),
            ('task2', 'task3', 'image'),
        )
        opt, label_to_taskid = optimize_task_graph(input, set())
        self.assertEqual(opt.graph, graph.Graph(
            {label_to_taskid['task2'], label_to_taskid['task3']},
            {(label_to_taskid['task2'], label_to_taskid['task3'], 'image')}))
