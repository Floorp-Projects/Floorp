# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from .. import target_tasks
from .. import try_option_syntax
from ..graph import Graph
from ..taskgraph import TaskGraph
from .util import TestTask
from mozunit import main


class FakeTryOptionSyntax(object):

    def __init__(self, message, task_graph):
        self.trigger_tests = 0
        self.notifications = None

    def task_matches(self, attributes):
        return 'at-at' in attributes


class TestTargetTasks(unittest.TestCase):

    def test_from_parameters(self):
        method = target_tasks.get_method('from_parameters')
        self.assertEqual(method(None, {'target_tasks': ['a', 'b']}),
                         ['a', 'b'])

    def default_matches(self, run_on_projects, project):
        method = target_tasks.get_method('default')
        graph = TaskGraph(tasks={
            'a': TestTask(kind='build', label='a',
                          attributes={'run_on_projects': run_on_projects}),
        }, graph=Graph(nodes={'a'}, edges=set()))
        parameters = {'project': project}
        return 'a' in method(graph, parameters)

    def test_default_all(self):
        """run_on_projects=[all] includes release, integration, and other projects"""
        self.assertTrue(self.default_matches(['all'], 'mozilla-central'))
        self.assertTrue(self.default_matches(['all'], 'mozilla-inbound'))
        self.assertTrue(self.default_matches(['all'], 'mozilla-aurora'))
        self.assertTrue(self.default_matches(['all'], 'baobab'))

    def test_default_integration(self):
        """run_on_projects=[integration] includes integration projects"""
        self.assertFalse(self.default_matches(['integration'], 'mozilla-central'))
        self.assertTrue(self.default_matches(['integration'], 'mozilla-inbound'))
        self.assertFalse(self.default_matches(['integration'], 'baobab'))

    def test_default_relesae(self):
        """run_on_projects=[release] includes release projects"""
        self.assertTrue(self.default_matches(['release'], 'mozilla-central'))
        self.assertFalse(self.default_matches(['release'], 'mozilla-inbound'))
        self.assertFalse(self.default_matches(['release'], 'baobab'))

    def test_default_nothing(self):
        """run_on_projects=[] includes nothing"""
        self.assertFalse(self.default_matches([], 'mozilla-central'))
        self.assertFalse(self.default_matches([], 'mozilla-inbound'))
        self.assertFalse(self.default_matches([], 'baobab'))

    def test_try_option_syntax(self):
        tasks = {
            'a': TestTask(kind=None, label='a'),
            'b': TestTask(kind=None, label='b', attributes={'at-at': 'yep'}),
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
