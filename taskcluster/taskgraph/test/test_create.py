# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys
import unittest
import os
import mock
import pytest

from taskgraph import create
from taskgraph.config import GraphConfig
from taskgraph.graph import Graph
from taskgraph.taskgraph import TaskGraph
from taskgraph.task import Task

from mozunit import main

GRAPH_CONFIG = GraphConfig({'trust-domain': 'domain'}, '/var/empty')


class TestCreate(unittest.TestCase):

    def setUp(self):
        self.old_task_id = os.environ.get('TASK_ID')
        if 'TASK_ID' in os.environ:
            del os.environ['TASK_ID']
        self.created_tasks = {}
        self.old_create_task = create.create_task
        create.create_task = self.fake_create_task

    def tearDown(self):
        create.create_task = self.old_create_task
        if self.old_task_id:
            os.environ['TASK_ID'] = self.old_task_id
        elif 'TASK_ID' in os.environ:
            del os.environ['TASK_ID']

    def fake_create_task(self, session, task_id, label, task_def):
        self.created_tasks[task_id] = task_def

    @pytest.mark.xfail(
        sys.version_info >= (3, 0), reason="python3 migration is not complete"
    )
    def test_create_tasks(self):
        tasks = {
            'tid-a': Task(kind='test', label='a', attributes={}, task={'payload': 'hello world'}),
            'tid-b': Task(kind='test', label='b', attributes={}, task={'payload': 'hello world'}),
        }
        label_to_taskid = {'a': 'tid-a', 'b': 'tid-b'}
        graph = Graph(nodes={'tid-a', 'tid-b'}, edges={('tid-a', 'tid-b', 'edge')})
        taskgraph = TaskGraph(tasks, graph)

        create.create_tasks(GRAPH_CONFIG, taskgraph, label_to_taskid, {'level': '4'})

        for tid, task in self.created_tasks.iteritems():
            self.assertEqual(task['payload'], 'hello world')
            self.assertEqual(task['schedulerId'], 'domain-level-4')
            # make sure the dependencies exist, at least
            for depid in task.get('dependencies', []):
                if depid == 'decisiontask':
                    # Don't look for decisiontask here
                    continue
                self.assertIn(depid, self.created_tasks)

    @pytest.mark.xfail(
        sys.version_info >= (3, 0), reason="python3 migration is not complete"
    )
    def test_create_task_without_dependencies(self):
        "a task with no dependencies depends on the decision task"
        os.environ['TASK_ID'] = 'decisiontask'
        tasks = {
            'tid-a': Task(kind='test', label='a', attributes={}, task={'payload': 'hello world'}),
        }
        label_to_taskid = {'a': 'tid-a'}
        graph = Graph(nodes={'tid-a'}, edges=set())
        taskgraph = TaskGraph(tasks, graph)

        create.create_tasks(GRAPH_CONFIG, taskgraph, label_to_taskid, {'level': '4'})

        for tid, task in self.created_tasks.iteritems():
            self.assertEqual(task.get('dependencies'), [os.environ['TASK_ID']])

    @pytest.mark.xfail(
        sys.version_info >= (3, 0), reason="python3 migration is not complete"
    )
    @mock.patch('taskgraph.create.create_task')
    def test_create_tasks_fails_if_create_fails(self, create_task):
        "creat_tasks fails if a single create_task call fails"
        os.environ['TASK_ID'] = 'decisiontask'
        tasks = {
            'tid-a': Task(kind='test', label='a', attributes={}, task={'payload': 'hello world'}),
        }
        label_to_taskid = {'a': 'tid-a'}
        graph = Graph(nodes={'tid-a'}, edges=set())
        taskgraph = TaskGraph(tasks, graph)

        def fail(*args):
            print("UHOH")
            raise RuntimeError('oh noes!')
        create_task.side_effect = fail

        with self.assertRaises(RuntimeError):
            create.create_tasks(GRAPH_CONFIG, taskgraph, label_to_taskid, {'level': '4'})


if __name__ == '__main__':
    main()
