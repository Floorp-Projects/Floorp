# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from taskgraph.graph import Graph
from taskgraph.task import Task
from taskgraph.taskgraph import TaskGraph
from mozunit import main


class TestTaskGraph(unittest.TestCase):

    maxDiff = None

    def test_taskgraph_to_json(self):
        tasks = {
            "a": Task(
                kind="test",
                label="a",
                description="Task A",
                attributes={"attr": "a-task"},
                task={"taskdef": True},
            ),
            "b": Task(
                kind="test",
                label="b",
                attributes={},
                task={"task": "def"},
                optimization={"skip-unless-has-relevant-tests": None},
                # note that this dep is ignored, superseded by that
                # from the taskgraph's edges
                dependencies={"first": "a"},
            ),
        }
        graph = Graph(nodes=set("ab"), edges={("a", "b", "edgelabel")})
        taskgraph = TaskGraph(tasks, graph)

        res = taskgraph.to_json()

        self.assertEqual(
            res,
            {
                "a": {
                    "kind": "test",
                    "label": "a",
                    "description": "Task A",
                    "attributes": {"attr": "a-task", "kind": "test"},
                    "task": {"taskdef": True},
                    "dependencies": {"edgelabel": "b"},
                    "soft_dependencies": [],
                    "if_dependencies": [],
                    "optimization": None,
                },
                "b": {
                    "kind": "test",
                    "label": "b",
                    "description": "",
                    "attributes": {"kind": "test"},
                    "task": {"task": "def"},
                    "dependencies": {},
                    "soft_dependencies": [],
                    "if_dependencies": [],
                    "optimization": {"skip-unless-has-relevant-tests": None},
                },
            },
        )

    def test_round_trip(self):
        graph = TaskGraph(
            tasks={
                "a": Task(
                    kind="fancy",
                    label="a",
                    description="Task A",
                    attributes={},
                    dependencies={"prereq": "b"},  # must match edges, below
                    optimization={"skip-unless-has-relevant-tests": None},
                    task={"task": "def"},
                ),
                "b": Task(
                    kind="pre",
                    label="b",
                    attributes={},
                    dependencies={},
                    optimization={"skip-unless-has-relevant-tests": None},
                    task={"task": "def2"},
                ),
            },
            graph=Graph(nodes={"a", "b"}, edges={("a", "b", "prereq")}),
        )

        tasks, new_graph = TaskGraph.from_json(graph.to_json())
        self.assertEqual(graph, new_graph)

    simple_graph = TaskGraph(
        tasks={
            "a": Task(
                kind="fancy",
                label="a",
                attributes={},
                dependencies={"prereq": "b"},  # must match edges, below
                optimization={"skip-unless-has-relevant-tests": None},
                task={"task": "def"},
            ),
            "b": Task(
                kind="pre",
                label="b",
                attributes={},
                dependencies={},
                optimization={"skip-unless-has-relevant-tests": None},
                task={"task": "def2"},
            ),
        },
        graph=Graph(nodes={"a", "b"}, edges={("a", "b", "prereq")}),
    )

    def test_contains(self):
        assert "a" in self.simple_graph
        assert "c" not in self.simple_graph


if __name__ == "__main__":
    main()
