# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..graph import Graph
from ..task.docker_image import DockerImageTask
from ..task.legacy import LegacyTask
from ..taskgraph import TaskGraph
from mozunit import main


class TestTargetTasks(unittest.TestCase):

    def test_from_json(self):
        legacy_dict = {
            'attributes': {'kind': 'legacy'},
            'task': {},
            'dependencies': {},
            'label': 'a',
            'kind_implementation': 'taskgraph.task.legacy:LegacyTask'
        }
        graph = TaskGraph(tasks={
            'a': LegacyTask(kind='legacy',
                            label='a',
                            attributes={},
                            task={},
                            task_dict=legacy_dict),
            'b': DockerImageTask(kind='docker-image',
                                 label='b',
                                 attributes={},
                                 task={"routes": []},
                                 index_paths=[]),
        }, graph=Graph(nodes={'a', 'b'}, edges=set()))

        tasks, new_graph = TaskGraph.from_json(graph.to_json(), "taskcluster/ci")
        self.assertEqual(graph.tasks['a'], new_graph.tasks['a'])
        self.assertEqual(graph, new_graph)

if __name__ == '__main__':
    main()
