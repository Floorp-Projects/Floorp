# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..generator import TaskGraphGenerator, Kind
from .. import graph
from ..task import base
from mozunit import main


class FakeTask(base.Task):

    def __init__(self, **kwargs):
        self.i = kwargs.pop('i')
        super(FakeTask, self).__init__(**kwargs)

    @classmethod
    def load_tasks(cls, kind, path, config, parameters, loaded_tasks):
        return [cls(kind=kind,
                    label='{}-t-{}'.format(kind, i),
                    attributes={'tasknum': str(i)},
                    task={},
                    i=i)
                for i in range(3)]

    def get_dependencies(self, full_task_set):
        i = self.i
        if i > 0:
            return [('{}-t-{}'.format(self.kind, i - 1), 'prev')]
        else:
            return []

    def optimize(self):
        return False, None


class FakeKind(Kind):

    def _get_impl_class(self):
        return FakeTask

    def load_tasks(self, parameters, loaded_tasks):
        FakeKind.loaded_kinds.append(self.name)
        return super(FakeKind, self).load_tasks(parameters, loaded_tasks)


class WithFakeKind(TaskGraphGenerator):

    def _load_kinds(self):
        for kind_name, deps in self.parameters['kinds']:
            yield FakeKind(
                kind_name, '/fake',
                {'kind-dependencies': deps} if deps else {})


class TestGenerator(unittest.TestCase):

    def maketgg(self, target_tasks=None, kinds=[('fake', [])]):
        FakeKind.loaded_kinds = []
        self.target_tasks = target_tasks or []

        def target_tasks_method(full_task_graph, parameters):
            return self.target_tasks
        return WithFakeKind('/root', {'kinds': kinds}, target_tasks_method)

    def test_kind_ordering(self):
        "When task kinds depend on each other, they are loaded in postorder"
        self.tgg = self.maketgg(kinds=[
            ('fake3', ['fake2', 'fake1']),
            ('fake2', ['fake1']),
            ('fake1', []),
        ])
        self.tgg._run_until('full_task_set')
        self.assertEqual(FakeKind.loaded_kinds, ['fake1', 'fake2', 'fake3'])

    def test_full_task_set(self):
        "The full_task_set property has all tasks"
        self.tgg = self.maketgg()
        self.assertEqual(self.tgg.full_task_set.graph,
                         graph.Graph({'fake-t-0', 'fake-t-1', 'fake-t-2'}, set()))
        self.assertEqual(sorted(self.tgg.full_task_set.tasks.keys()),
                         sorted(['fake-t-0', 'fake-t-1', 'fake-t-2']))

    def test_full_task_graph(self):
        "The full_task_graph property has all tasks, and links"
        self.tgg = self.maketgg()
        self.assertEqual(self.tgg.full_task_graph.graph,
                         graph.Graph({'fake-t-0', 'fake-t-1', 'fake-t-2'},
                                     {
                                         ('fake-t-1', 'fake-t-0', 'prev'),
                                         ('fake-t-2', 'fake-t-1', 'prev'),
                         }))
        self.assertEqual(sorted(self.tgg.full_task_graph.tasks.keys()),
                         sorted(['fake-t-0', 'fake-t-1', 'fake-t-2']))

    def test_target_task_set(self):
        "The target_task_set property has the targeted tasks"
        self.tgg = self.maketgg(['fake-t-1'])
        self.assertEqual(self.tgg.target_task_set.graph,
                         graph.Graph({'fake-t-1'}, set()))
        self.assertEqual(self.tgg.target_task_set.tasks.keys(),
                         ['fake-t-1'])

    def test_target_task_graph(self):
        "The target_task_graph property has the targeted tasks and deps"
        self.tgg = self.maketgg(['fake-t-1'])
        self.assertEqual(self.tgg.target_task_graph.graph,
                         graph.Graph({'fake-t-0', 'fake-t-1'},
                                     {('fake-t-1', 'fake-t-0', 'prev')}))
        self.assertEqual(sorted(self.tgg.target_task_graph.tasks.keys()),
                         sorted(['fake-t-0', 'fake-t-1']))

    def test_optimized_task_graph(self):
        "The optimized task graph contains task ids"
        self.tgg = self.maketgg(['fake-t-2'])
        tid = self.tgg.label_to_taskid
        self.assertEqual(
            self.tgg.optimized_task_graph.graph,
            graph.Graph({tid['fake-t-0'], tid['fake-t-1'], tid['fake-t-2']}, {
                (tid['fake-t-1'], tid['fake-t-0'], 'prev'),
                (tid['fake-t-2'], tid['fake-t-1'], 'prev'),
            }))

if __name__ == '__main__':
    main()
