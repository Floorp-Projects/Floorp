# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import pytest
import unittest
from mozunit import main

from taskgraph.generator import TaskGraphGenerator, Kind
from taskgraph.optimize import OptimizationStrategy
from taskgraph.util.templates import merge
from taskgraph import (
    generator,
    graph,
    optimize as optimize_mod,
    target_tasks as target_tasks_mod,
)


def fake_loader(kind, path, config, parameters, loaded_tasks):
    for i in range(3):
        dependencies = {}
        if i >= 1:
            dependencies['prev'] = '{}-t-{}'.format(kind, i-1)

        task = {
            'kind': kind,
            'label': '{}-t-{}'.format(kind, i),
            'attributes': {'_tasknum': str(i)},
            'task': {'i': i},
            'dependencies': dependencies,
        }
        if 'job-defaults' in config:
            task = merge(config['job-defaults'], task)
        yield task


class FakeKind(Kind):

    def _get_loader(self):
        return fake_loader

    def load_tasks(self, parameters, loaded_tasks):
        FakeKind.loaded_kinds.append(self.name)
        return super(FakeKind, self).load_tasks(parameters, loaded_tasks)


class WithFakeKind(TaskGraphGenerator):

    def _load_kinds(self, graph_config):
        for kind_name, cfg in self.parameters['_kinds']:
            config = {
                'transforms': [],
            }
            if cfg:
                config.update(cfg)
            yield FakeKind(kind_name, '/fake', config, graph_config)


def fake_load_graph_config(root_dir):
    return {'trust-domain': 'test-domain'}


class FakeParameters(dict):
    strict = True


class FakeOptimization(OptimizationStrategy):
    def __init__(self, mode, *args, **kwargs):
        super(FakeOptimization, self).__init__(*args, **kwargs)
        self.mode = mode

    def should_remove_task(self, task, params, arg):
        if self.mode == 'always':
            return True
        if self.mode == 'even':
            return task.task['i'] % 2 == 0
        if self.mode == 'odd':
            return task.task['i'] % 2 != 0
        return False


class TestGenerator(unittest.TestCase):

    @pytest.fixture(autouse=True)
    def patch(self, monkeypatch):
        self.patch = monkeypatch

    def maketgg(self, target_tasks=None, kinds=[('_fake', [])], params=None):
        params = params or {}
        FakeKind.loaded_kinds = []
        self.target_tasks = target_tasks or []

        def target_tasks_method(full_task_graph, parameters, graph_config):
            return self.target_tasks

        fake_registry = {mode: FakeOptimization(mode)
                         for mode in ('always', 'never', 'even', 'odd')}

        target_tasks_mod._target_task_methods['test_method'] = target_tasks_method
        self.patch.setattr(optimize_mod, 'registry', fake_registry)

        parameters = FakeParameters({
            '_kinds': kinds,
            'target_tasks_method': 'test_method',
            'try_mode': None,
        })
        parameters.update(params)

        self.patch.setattr(generator, 'load_graph_config', fake_load_graph_config)

        return WithFakeKind('/root', parameters)

    def test_kind_ordering(self):
        "When task kinds depend on each other, they are loaded in postorder"
        self.tgg = self.maketgg(kinds=[
            ('_fake3', {'kind-dependencies': ['_fake2', '_fake1']}),
            ('_fake2', {'kind-dependencies': ['_fake1']}),
            ('_fake1', {'kind-dependencies': []}),
        ])
        self.tgg._run_until('full_task_set')
        self.assertEqual(FakeKind.loaded_kinds, ['_fake1', '_fake2', '_fake3'])

    def test_full_task_set(self):
        "The full_task_set property has all tasks"
        self.tgg = self.maketgg()
        self.assertEqual(self.tgg.full_task_set.graph,
                         graph.Graph({'_fake-t-0', '_fake-t-1', '_fake-t-2'}, set()))
        self.assertEqual(sorted(self.tgg.full_task_set.tasks.keys()),
                         sorted(['_fake-t-0', '_fake-t-1', '_fake-t-2']))

    def test_full_task_graph(self):
        "The full_task_graph property has all tasks, and links"
        self.tgg = self.maketgg()
        self.assertEqual(self.tgg.full_task_graph.graph,
                         graph.Graph({'_fake-t-0', '_fake-t-1', '_fake-t-2'},
                                     {
                                         ('_fake-t-1', '_fake-t-0', 'prev'),
                                         ('_fake-t-2', '_fake-t-1', 'prev'),
                         }))
        self.assertEqual(sorted(self.tgg.full_task_graph.tasks.keys()),
                         sorted(['_fake-t-0', '_fake-t-1', '_fake-t-2']))

    def test_target_task_set(self):
        "The target_task_set property has the targeted tasks"
        self.tgg = self.maketgg(['_fake-t-1'])
        self.assertEqual(self.tgg.target_task_set.graph,
                         graph.Graph({'_fake-t-1'}, set()))
        self.assertEqual(self.tgg.target_task_set.tasks.keys(),
                         ['_fake-t-1'])

    def test_target_task_graph(self):
        "The target_task_graph property has the targeted tasks and deps"
        self.tgg = self.maketgg(['_fake-t-1'])
        self.assertEqual(self.tgg.target_task_graph.graph,
                         graph.Graph({'_fake-t-0', '_fake-t-1'},
                                     {('_fake-t-1', '_fake-t-0', 'prev')}))
        self.assertEqual(sorted(self.tgg.target_task_graph.tasks.keys()),
                         sorted(['_fake-t-0', '_fake-t-1']))

    def test_always_target_tasks(self):
        "The target_task_graph includes tasks with 'always_target'"
        tgg_args = {
            'target_tasks': ['_fake-t-0', '_fake-t-1', '_ignore-t-0', '_ignore-t-1'],
            'kinds': [
                ('_fake', {'job-defaults': {'optimization': {'odd': None}}}),
                ('_ignore', {'job-defaults': {
                    'attributes': {'always_target': True},
                    'optimization': {'even': None},
                }}),
            ],
            'params': {'optimize_target_tasks': False},
        }
        self.tgg = self.maketgg(**tgg_args)
        self.assertEqual(
            sorted(self.tgg.target_task_set.tasks.keys()),
            sorted(['_fake-t-0', '_fake-t-1', '_ignore-t-0', '_ignore-t-1']))
        self.assertEqual(
            sorted(self.tgg.target_task_graph.tasks.keys()),
            sorted(['_fake-t-0', '_fake-t-1', '_ignore-t-0', '_ignore-t-1', '_ignore-t-2']))
        self.assertEqual(
            sorted([t.label for t in self.tgg.optimized_task_graph.tasks.values()]),
            sorted(['_fake-t-0', '_fake-t-1', '_ignore-t-0', '_ignore-t-1']))

    def test_optimized_task_graph(self):
        "The optimized task graph contains task ids"
        self.tgg = self.maketgg(['_fake-t-2'])
        tid = self.tgg.label_to_taskid
        self.assertEqual(
            self.tgg.optimized_task_graph.graph,
            graph.Graph({tid['_fake-t-0'], tid['_fake-t-1'], tid['_fake-t-2']}, {
                (tid['_fake-t-1'], tid['_fake-t-0'], 'prev'),
                (tid['_fake-t-2'], tid['_fake-t-1'], 'prev'),
            }))


if __name__ == '__main__':
    main()
