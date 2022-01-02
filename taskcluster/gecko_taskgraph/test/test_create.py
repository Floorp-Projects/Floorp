# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import unittest

from unittest import mock

from gecko_taskgraph import create
from gecko_taskgraph.config import GraphConfig
from gecko_taskgraph.graph import Graph
from gecko_taskgraph.taskgraph import TaskGraph
from gecko_taskgraph.task import Task

from mozunit import main

GRAPH_CONFIG = GraphConfig({"trust-domain": "domain"}, "/var/empty")


class TestCreate(unittest.TestCase):
    def setUp(self):
        self.created_tasks = {}
        self.old_create_task = create.create_task
        create.create_task = self.fake_create_task

    def tearDown(self):
        create.create_task = self.old_create_task

    def fake_create_task(self, session, task_id, label, task_def):
        self.created_tasks[task_id] = task_def

    def test_create_tasks(self):
        tasks = {
            "tid-a": Task(
                kind="test", label="a", attributes={}, task={"payload": "hello world"}
            ),
            "tid-b": Task(
                kind="test", label="b", attributes={}, task={"payload": "hello world"}
            ),
        }
        label_to_taskid = {"a": "tid-a", "b": "tid-b"}
        graph = Graph(nodes={"tid-a", "tid-b"}, edges={("tid-a", "tid-b", "edge")})
        taskgraph = TaskGraph(tasks, graph)

        create.create_tasks(
            GRAPH_CONFIG,
            taskgraph,
            label_to_taskid,
            {"level": "4"},
            decision_task_id="decisiontask",
        )

        for tid, task in self.created_tasks.items():
            self.assertEqual(task["payload"], "hello world")
            self.assertEqual(task["schedulerId"], "domain-level-4")
            # make sure the dependencies exist, at least
            for depid in task.get("dependencies", []):
                if depid == "decisiontask":
                    # Don't look for decisiontask here
                    continue
                self.assertIn(depid, self.created_tasks)

    def test_create_task_without_dependencies(self):
        "a task with no dependencies depends on the decision task"
        tasks = {
            "tid-a": Task(
                kind="test", label="a", attributes={}, task={"payload": "hello world"}
            ),
        }
        label_to_taskid = {"a": "tid-a"}
        graph = Graph(nodes={"tid-a"}, edges=set())
        taskgraph = TaskGraph(tasks, graph)

        create.create_tasks(
            GRAPH_CONFIG,
            taskgraph,
            label_to_taskid,
            {"level": "4"},
            decision_task_id="decisiontask",
        )

        for tid, task in self.created_tasks.items():
            self.assertEqual(task.get("dependencies"), ["decisiontask"])

    @mock.patch("gecko_taskgraph.create.create_task")
    def test_create_tasks_fails_if_create_fails(self, create_task):
        "creat_tasks fails if a single create_task call fails"
        tasks = {
            "tid-a": Task(
                kind="test", label="a", attributes={}, task={"payload": "hello world"}
            ),
        }
        label_to_taskid = {"a": "tid-a"}
        graph = Graph(nodes={"tid-a"}, edges=set())
        taskgraph = TaskGraph(tasks, graph)

        def fail(*args):
            print("UHOH")
            raise RuntimeError("oh noes!")

        create_task.side_effect = fail

        with self.assertRaises(RuntimeError):
            create.create_tasks(
                GRAPH_CONFIG,
                taskgraph,
                label_to_taskid,
                {"level": "4"},
                decision_task_id="decisiontask",
            )


if __name__ == "__main__":
    main()
