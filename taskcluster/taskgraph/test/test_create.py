# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
import os

from .. import create
from ..graph import Graph
from ..types import Task, TaskGraph

from mozunit import main

class FakeKind(object):

    def get_task_definition(self, task, deps_by_name):
        # sanity-check the deps_by_name
        for k, v in deps_by_name.iteritems():
            assert k == 'edge'
        return {'payload': 'hello world'}


class TestCreate(unittest.TestCase):

    def setUp(self):
        self.old_task_id = os.environ.get('TASK_ID')
        if 'TASK_ID' in os.environ:
            del os.environ['TASK_ID']
        self.created_tasks = {}
        self.old_create_task = create._create_task
        create._create_task = self.fake_create_task

    def tearDown(self):
        create._create_task = self.old_create_task
        if self.old_task_id:
            os.environ['TASK_ID'] = self.old_task_id
        elif 'TASK_ID' in os.environ:
            del os.environ['TASK_ID']

    def fake_create_task(self, session, task_id, label, task_def):
        self.created_tasks[task_id] = task_def

    def test_create_tasks(self):
        kind = FakeKind()
        tasks = {
            'tid-a': Task(kind=kind, label='a', task={'payload': 'hello world'}),
            'tid-b': Task(kind=kind, label='b', task={'payload': 'hello world'}),
        }
        label_to_taskid = {'a': 'tid-a', 'b': 'tid-b'}
        graph = Graph(nodes={'tid-a', 'tid-b'}, edges={('tid-a', 'tid-b', 'edge')})
        taskgraph = TaskGraph(tasks, graph)

        create.create_tasks(taskgraph, label_to_taskid)

        for tid, task in self.created_tasks.iteritems():
            self.assertEqual(task['payload'], 'hello world')
            # make sure the dependencies exist, at least
            for depid in task.get('dependencies', []):
                self.assertIn(depid, self.created_tasks)

    def test_create_task_without_dependencies(self):
        "a task with no dependencies depends on the decision task"
        os.environ['TASK_ID'] = 'decisiontask'
        kind = FakeKind()
        tasks = {
            'tid-a': Task(kind=kind, label='a', task={'payload': 'hello world'}),
        }
        label_to_taskid = {'a': 'tid-a'}
        graph = Graph(nodes={'tid-a'}, edges=set())
        taskgraph = TaskGraph(tasks, graph)

        create.create_tasks(taskgraph, label_to_taskid)

        for tid, task in self.created_tasks.iteritems():
            self.assertEqual(task['dependencies'], [os.environ['TASK_ID']])


if __name__ == '__main__':
    main()

