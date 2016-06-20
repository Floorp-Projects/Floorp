# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..generator import TaskGraphGenerator
from .. import graph
from ..kind import base
from mozunit import main


class FakeTask(base.Task):

    def __init__(self, **kwargs):
        self.i = kwargs.pop('i')
        super(FakeTask, self).__init__(**kwargs)

    @classmethod
    def load_tasks(cls, kind, path, config, parameters):
        return [cls(kind=kind,
                    label='t-{}'.format(i),
                    attributes={'tasknum': str(i)},
                    task={},
                    i=i)
                for i in range(3)]

    def get_dependencies(self, full_task_set):
        i = self.i
        if i > 0:
            return [('t-{}'.format(i - 1), 'prev')]
        else:
            return []

    def optimize(self):
        return False, None


class WithFakeTask(TaskGraphGenerator):

    def _load_kinds(self):
        return FakeTask.load_tasks('fake', '/fake', {}, {})


class TestGenerator(unittest.TestCase):

    def setUp(self):
        self.target_tasks = []

        def target_tasks_method(full_task_graph, parameters):
            return self.target_tasks
        self.tgg = WithFakeTask('/root', {}, target_tasks_method)

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
        "The optimized task graph contains task ids"
        self.target_tasks = ['t-2']
        tid = self.tgg.label_to_taskid
        self.assertEqual(
            self.tgg.optimized_task_graph.graph,
            graph.Graph({tid['t-0'], tid['t-1'], tid['t-2']}, {
                (tid['t-1'], tid['t-0'], 'prev'),
                (tid['t-2'], tid['t-1'], 'prev'),
            })
            )

if __name__ == '__main__':
    main()
