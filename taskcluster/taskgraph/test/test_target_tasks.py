# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import unittest

from taskgraph import target_tasks
from taskgraph import try_option_syntax
from taskgraph.graph import Graph
from taskgraph.taskgraph import TaskGraph
from taskgraph.task import Task
from mozunit import main


class FakeTryOptionSyntax(object):

    def __init__(self, message, task_graph):
        self.trigger_tests = 0
        self.talos_trigger_tests = 0
        self.notifications = None
        self.env = []
        self.profile = False
        self.tag = None
        self.no_retry = False

    def task_matches(self, task):
        return 'at-at' in task.attributes


class TestTargetTasks(unittest.TestCase):

    def default_matches(self, run_on_projects, project):
        method = target_tasks.get_method('default')
        graph = TaskGraph(tasks={
            'a': Task(kind='build', label='a',
                      attributes={'run_on_projects': run_on_projects},
                      task={}),
        }, graph=Graph(nodes={'a'}, edges=set()))
        parameters = {'project': project}
        return 'a' in method(graph, parameters)

    def test_default_all(self):
        """run_on_projects=[all] includes release, integration, and other projects"""
        self.assertTrue(self.default_matches(['all'], 'mozilla-central'))
        self.assertTrue(self.default_matches(['all'], 'mozilla-inbound'))
        self.assertTrue(self.default_matches(['all'], 'baobab'))

    def test_default_integration(self):
        """run_on_projects=[integration] includes integration projects"""
        self.assertFalse(self.default_matches(['integration'], 'mozilla-central'))
        self.assertTrue(self.default_matches(['integration'], 'mozilla-inbound'))
        self.assertFalse(self.default_matches(['integration'], 'baobab'))

    def test_default_release(self):
        """run_on_projects=[release] includes release projects"""
        self.assertTrue(self.default_matches(['release'], 'mozilla-central'))
        self.assertFalse(self.default_matches(['release'], 'mozilla-inbound'))
        self.assertFalse(self.default_matches(['release'], 'baobab'))

    def test_default_nothing(self):
        """run_on_projects=[] includes nothing"""
        self.assertFalse(self.default_matches([], 'mozilla-central'))
        self.assertFalse(self.default_matches([], 'mozilla-inbound'))
        self.assertFalse(self.default_matches([], 'baobab'))

    def test_try_tasks(self):
        tasks = {
            'a': Task(kind=None, label='a', attributes={}, task={}),
            'b': Task(kind=None, label='b', attributes={'at-at': 'yep'}, task={}),
            'c': Task(kind=None, label='c', attributes={}, task={}),
        }
        graph = Graph(nodes=set('abc'), edges=set())
        tg = TaskGraph(tasks, graph)

        method = target_tasks.get_method('try_tasks')
        config = os.path.join(os.getcwd(), 'try_task_config.json')

        orig_TryOptionSyntax = try_option_syntax.TryOptionSyntax
        try:
            try_option_syntax.TryOptionSyntax = FakeTryOptionSyntax

            # no try specifier
            self.assertEqual(method(tg, {'message': ''}), ['b'])

            # try syntax only
            self.assertEqual(method(tg, {'message': 'try: me'}), ['b'])

            # try task config only
            with open(config, 'w') as fh:
                fh.write('["c"]')
            self.assertEqual(method(tg, {'message': ''}), ['c'])

            with open(config, 'w') as fh:
                fh.write('{"c": {}}')
            self.assertEqual(method(tg, {'message': ''}), ['c'])

            # both syntax and config
            self.assertEqual(set(method(tg, {'message': 'try: me'})), set(['b', 'c']))
        finally:
            try_option_syntax.TryOptionSyntax = orig_TryOptionSyntax
            if os.path.isfile(config):
                os.remove(config)


if __name__ == '__main__':
    main()
