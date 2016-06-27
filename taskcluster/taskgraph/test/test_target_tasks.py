# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from .. import target_tasks
from .. import try_option_syntax
from ..graph import Graph
from ..types import Task, TaskGraph
from mozunit import main


class FakeTryOptionSyntax(object):

    def __init__(self, message, task_graph):
        pass

    def task_matches(self, attributes):
        return 'at-at' in attributes


class TestTargetTasks(unittest.TestCase):

    def test_from_parameters(self):
        method = target_tasks.get_method('from_parameters')
        self.assertEqual(method(None, {'target_tasks': ['a', 'b']}),
                         ['a', 'b'])

    def test_all_builds_and_tests(self):
        method = target_tasks.get_method('all_builds_and_tests')
        graph = TaskGraph(tasks={
            'a': Task(kind=None, label='a', attributes={'kind': 'legacy'}),
            'b': Task(kind=None, label='b', attributes={'kind': 'legacy'}),
            'boring': Task(kind=None, label='boring', attributes={'kind': 'docker-image'}),
        }, graph=Graph(nodes={'a', 'b', 'boring'}, edges=set()))
        self.assertEqual(sorted(method(graph, {})), sorted(['a', 'b']))

    def test_try_option_syntax(self):
        tasks = {
            'a': Task(kind=None, label='a'),
            'b': Task(kind=None, label='b', attributes={'at-at': 'yep'}),
        }
        graph = Graph(nodes=set('ab'), edges=set())
        tg = TaskGraph(tasks, graph)
        params = {'message': 'try me'}

        orig_TryOptionSyntax = try_option_syntax.TryOptionSyntax
        try:
            try_option_syntax.TryOptionSyntax = FakeTryOptionSyntax
            method = target_tasks.get_method('try_option_syntax')
            self.assertEqual(method(tg, params), ['b'])
        finally:
            try_option_syntax.TryOptionSyntax = orig_TryOptionSyntax

if __name__ == '__main__':
    main()
