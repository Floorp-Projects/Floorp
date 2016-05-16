# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

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
        self.created_tasks = {}
        self.old_create_task = create._create_task
        create._create_task = self.fake_create_task

    def tearDown(self):
        create._create_task = self.old_create_task

    def fake_create_task(self, session, task_id, label, task_def):
        self.created_tasks[task_id] = task_def

    def test_create_tasks(self):
        kind = FakeKind()
        tasks = {
            'a': Task(kind=kind, label='a'),
            'b': Task(kind=kind, label='b'),
        }
        graph = Graph(nodes=set('ab'), edges={('a', 'b', 'edge')})
        taskgraph = TaskGraph(tasks, graph)

        create.create_tasks(taskgraph)

        for tid, task in self.created_tasks.iteritems():
            self.assertEqual(task['payload'], 'hello world')
            # make sure the dependencies exist, at least
            for depid in task['dependencies']:
                self.assertIn(depid, self.created_tasks)


if __name__ == '__main__':
    main()

