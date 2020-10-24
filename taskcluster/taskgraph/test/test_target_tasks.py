# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import contextlib
import unittest

import pytest
from mozunit import main

from taskgraph import target_tasks
from taskgraph import try_option_syntax
from taskgraph.graph import Graph
from taskgraph.taskgraph import TaskGraph
from taskgraph.task import Task


class FakeTryOptionSyntax(object):

    def __init__(self, message, task_graph, graph_config):
        self.trigger_tests = 0
        self.talos_trigger_tests = 0
        self.raptor_trigger_tests = 0
        self.notifications = None
        self.env = []
        self.profile = False
        self.tag = None
        self.no_retry = False

    def task_matches(self, task):
        return 'at-at' in task.attributes


class TestTargetTasks(unittest.TestCase):

    def default_matches_project(self, run_on_projects, project):
        return self.default_matches(
            attributes={
                'run_on_projects': run_on_projects,
            },
            parameters={
                'project': project,
                'hg_branch': 'default',
            },
        )

    def default_matches_hg_branch(self, run_on_hg_branches, hg_branch):
        attributes = {'run_on_projects': ['all']}
        if run_on_hg_branches is not None:
            attributes['run_on_hg_branches'] = run_on_hg_branches

        return self.default_matches(
            attributes=attributes,
            parameters={
                'project': 'mozilla-central',
                'hg_branch': hg_branch,
            },
        )

    def default_matches(self, attributes, parameters):
        method = target_tasks.get_method('default')
        graph = TaskGraph(tasks={
            'a': Task(kind='build', label='a',
                      attributes=attributes,
                      task={}),
        }, graph=Graph(nodes={'a'}, edges=set()))
        return 'a' in method(graph, parameters, {})

    def test_default_all(self):
        """run_on_projects=[all] includes release, integration, and other projects"""
        self.assertTrue(self.default_matches_project(['all'], 'mozilla-central'))
        self.assertTrue(self.default_matches_project(['all'], 'baobab'))

    def test_default_integration(self):
        """run_on_projects=[integration] includes integration projects"""
        self.assertFalse(self.default_matches_project(['integration'], 'mozilla-central'))
        self.assertFalse(self.default_matches_project(['integration'], 'baobab'))

    def test_default_release(self):
        """run_on_projects=[release] includes release projects"""
        self.assertTrue(self.default_matches_project(['release'], 'mozilla-central'))
        self.assertFalse(self.default_matches_project(['release'], 'baobab'))

    def test_default_nothing(self):
        """run_on_projects=[] includes nothing"""
        self.assertFalse(self.default_matches_project([], 'mozilla-central'))
        self.assertFalse(self.default_matches_project([], 'baobab'))

    def test_default_hg_branch(self):
        self.assertTrue(self.default_matches_hg_branch(None, 'default'))
        self.assertTrue(self.default_matches_hg_branch(None, 'GECKOVIEW_62_RELBRANCH'))

        self.assertFalse(self.default_matches_hg_branch([], 'default'))
        self.assertFalse(self.default_matches_hg_branch([], 'GECKOVIEW_62_RELBRANCH'))

        self.assertTrue(self.default_matches_hg_branch(['all'], 'default'))
        self.assertTrue(self.default_matches_hg_branch(['all'], 'GECKOVIEW_62_RELBRANCH'))

        self.assertTrue(self.default_matches_hg_branch(['default'], 'default'))
        self.assertTrue(self.default_matches_hg_branch([r'default'], 'default'))
        self.assertFalse(self.default_matches_hg_branch([r'default'], 'GECKOVIEW_62_RELBRANCH'))

        self.assertTrue(
            self.default_matches_hg_branch(['GECKOVIEW_62_RELBRANCH'], 'GECKOVIEW_62_RELBRANCH')
        )
        self.assertTrue(
            self.default_matches_hg_branch(['GECKOVIEW_\d+_RELBRANCH'], 'GECKOVIEW_62_RELBRANCH')
        )
        self.assertTrue(
            self.default_matches_hg_branch([r'GECKOVIEW_\d+_RELBRANCH'], 'GECKOVIEW_62_RELBRANCH')
        )
        self.assertFalse(self.default_matches_hg_branch([r'GECKOVIEW_\d+_RELBRANCH'], 'default'))

    def make_task_graph(self):
        tasks = {
            'a': Task(kind=None, label='a', attributes={}, task={}),
            'b': Task(kind=None, label='b', attributes={'at-at': 'yep'}, task={}),
            'c': Task(kind=None, label='c', attributes={'run_on_projects': ['try']}, task={}),
        }
        graph = Graph(nodes=set('abc'), edges=set())
        return TaskGraph(tasks, graph)

    @contextlib.contextmanager
    def fake_TryOptionSyntax(self):
        orig_TryOptionSyntax = try_option_syntax.TryOptionSyntax
        try:
            try_option_syntax.TryOptionSyntax = FakeTryOptionSyntax
            yield
        finally:
            try_option_syntax.TryOptionSyntax = orig_TryOptionSyntax

    def test_empty_try(self):
        "try_mode = None runs nothing"
        tg = self.make_task_graph()
        method = target_tasks.get_method('try_tasks')
        params = {
            'try_mode': None,
            'project': 'try',
            'message': '',
        }
        # only runs the task with run_on_projects: try
        self.assertEqual(method(tg, params, {}), [])

    def test_try_option_syntax(self):
        "try_mode = try_option_syntax uses TryOptionSyntax"
        tg = self.make_task_graph()
        method = target_tasks.get_method('try_tasks')
        with self.fake_TryOptionSyntax():
            params = {
                'try_mode': 'try_option_syntax',
                'message': 'try: -p all',
            }
            self.assertEqual(method(tg, params, {}), ['b'])

    def test_try_task_config(self):
        "try_mode = try_task_config uses the try config"
        tg = self.make_task_graph()
        method = target_tasks.get_method('try_tasks')
        params = {
            'try_mode': 'try_task_config',
            'try_task_config': {'tasks': ['a']},
        }
        self.assertEqual(method(tg, params, {}), ['a'])


# tests for specific filters

@pytest.mark.parametrize(
    "name,task,params,expected",
    (
        pytest.param(
            "filter_tests_without_manifests",
            Task(kind="test", label="a", attributes={}, task={}),
            None,
            True,
            id="filter_tests_without_manifests_not_in_attributes",
        ),
        pytest.param(
            "filter_tests_without_manifests",
            Task(kind="test", label="a", attributes={'test_manifests': ['foo']}, task={}),
            None,
            True,
            id="filter_tests_without_manifests_has_test_manifests",
        ),
        pytest.param(
            "filter_tests_without_manifests",
            Task(kind="build", label="a", attributes={'test_manifests': None}, task={}),
            None,
            True,
            id="filter_tests_without_manifests_not_a_test",
        ),
        pytest.param(
            "filter_tests_without_manifests",
            Task(kind="test", label="a", attributes={'test_manifests': None}, task={}),
            None,
            False,
            id="filter_tests_without_manifests_has_no_test_manifests",
        ),
    )
)
def test_filters(name, task, params, expected):
    func = getattr(target_tasks, name)
    assert func(task, params) is expected


if __name__ == '__main__':
    main()
