# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..generator import TaskGraphGenerator
from .. import types
from .. import graph
from mozunit import main


class FakeKind(object):

    def maketask(self, i):
        return types.Task(
            self,
            label='t-{}'.format(i),
            attributes={'tasknum': str(i)},
            task={},
            i=i)

    def load_tasks(self, parameters):
        self.tasks = [self.maketask(i) for i in range(3)]
        return self.tasks

    def get_task_dependencies(self, task, full_task_set):
        i = task.extra['i']
        if i > 0:
            return [('t-{}'.format(i - 1), 'prev')]
        else:
            return []


class WithFakeKind(TaskGraphGenerator):

    def _load_kinds(self):
        yield FakeKind()


class TestGenerator(unittest.TestCase):

    def setUp(self):
        self.target_tasks = []

        def target_tasks_method(full_task_graph, parameters):
            return self.target_tasks
        self.tgg = WithFakeKind('/root', {}, target_tasks_method)

    def test_full_task_set(self):
        "The full_task_set property has all tasks"
        self.assertEqual(self.tgg.full_task_set.graph,
                         graph.Graph({'t-0', 't-1', 't-2'}, set()))
        self.assertEqual(self.tgg.full_task_set.tasks.keys(),
                         ['t-0', 't-1', 't-2'])

    def test_full_task_graph(self):
        "The full_task_graph property has all tasks, and links"
        self.assertEqual(self.tgg.full_task_graph.graph,
                         graph.Graph({'t-0', 't-1', 't-2'},
                                     {
                                         ('t-1', 't-0', 'prev'),
                                         ('t-2', 't-1', 'prev'),
                         }))
        self.assertEqual(self.tgg.full_task_graph.tasks.keys(),
                         ['t-0', 't-1', 't-2'])

    def test_target_task_set(self):
        "The target_task_set property has the targeted tasks"
        self.target_tasks = ['t-1']
        self.assertEqual(self.tgg.target_task_set.graph,
                         graph.Graph({'t-1'}, set()))
        self.assertEqual(self.tgg.target_task_set.tasks.keys(),
                         ['t-1'])

    def test_target_task_graph(self):
        "The target_task_graph property has the targeted tasks and deps"
        self.target_tasks = ['t-1']
        self.assertEqual(self.tgg.target_task_graph.graph,
                         graph.Graph({'t-0', 't-1'},
                                     {('t-1', 't-0', 'prev')}))
        self.assertEqual(sorted(self.tgg.target_task_graph.tasks.keys()),
                         sorted(['t-0', 't-1']))

    def test_optimized_task_graph(self):
        "The optimized task graph is the target task graph (for now)"
        self.target_tasks = ['t-1']
        self.assertEqual(self.tgg.optimized_task_graph.graph,
                         self.tgg.target_task_graph.graph)

if __name__ == '__main__':
    main()
