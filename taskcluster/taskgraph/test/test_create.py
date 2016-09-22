# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
import os

from .. import create
from ..graph import Graph
from ..taskgraph import TaskGraph
from .util import TestTask

from mozunit import main


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
        tasks = {
            'tid-a': TestTask(label='a', task={'payload': 'hello world'}),
            'tid-b': TestTask(label='b', task={'payload': 'hello world'}),
        }
        label_to_taskid = {'a': 'tid-a', 'b': 'tid-b'}
        graph = Graph(nodes={'tid-a', 'tid-b'}, edges={('tid-a', 'tid-b', 'edge')})
        taskgraph = TaskGraph(tasks, graph)

        create.create_tasks(taskgraph, label_to_taskid, {'level': '4'})

        for tid, task in self.created_tasks.iteritems():
            self.assertEqual(task['payload'], 'hello world')
            self.assertEqual(task['schedulerId'], 'gecko-level-4')
            # make sure the dependencies exist, at least
            for depid in task.get('dependencies', []):
                if depid is 'decisiontask':
                    # Don't look for decisiontask here
                    continue
                self.assertIn(depid, self.created_tasks)

    def test_create_task_without_dependencies(self):
        "a task with no dependencies depends on the decision task"
        os.environ['TASK_ID'] = 'decisiontask'
        tasks = {
            'tid-a': TestTask(label='a', task={'payload': 'hello world'}),
        }
        label_to_taskid = {'a': 'tid-a'}
        graph = Graph(nodes={'tid-a'}, edges=set())
        taskgraph = TaskGraph(tasks, graph)

        create.create_tasks(taskgraph, label_to_taskid, {'level': '4'})

        for tid, task in self.created_tasks.iteritems():
            self.assertEqual(task.get('dependencies'), [os.environ['TASK_ID']])


if __name__ == '__main__':
    main()
