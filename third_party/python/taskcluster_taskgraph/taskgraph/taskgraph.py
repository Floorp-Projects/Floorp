# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import attr

from .graph import Graph
from .task import Task


@attr.s(frozen=True)
class TaskGraph:
    """
    Representation of a task graph.

    A task graph is a combination of a Graph and a dictionary of tasks indexed
    by label. TaskGraph instances should be treated as immutable.

    In the graph, tasks are said to "link to" their dependencies. Whereas
    tasks are "linked from" their dependents.
    """

    tasks = attr.ib()
    graph = attr.ib()

    def __attrs_post_init__(self):
        assert set(self.tasks) == self.graph.nodes

    def for_each_task(self, f, *args, **kwargs):
        for task_label in self.graph.visit_postorder():
            task = self.tasks[task_label]
            f(task, self, *args, **kwargs)

    def __getitem__(self, label):
        "Get a task by label"
        return self.tasks[label]

    def __contains__(self, label):
        return label in self.tasks

    def __iter__(self):
        "Iterate over tasks in undefined order"
        return iter(self.tasks.values())

    def to_json(self):
        "Return a JSON-able object representing the task graph, as documented"
        named_links_dict = self.graph.named_links_dict()
        # this dictionary may be keyed by label or by taskid, so let's just call it 'key'
        tasks = {}
        for key in self.graph.visit_postorder():
            tasks[key] = self.tasks[key].to_json()
            # overwrite dependencies with the information in the taskgraph's edges.
            tasks[key]["dependencies"] = named_links_dict.get(key, {})
        return tasks

    @classmethod
    def from_json(cls, tasks_dict):
        """
        This code is used to generate the a TaskGraph using a dictionary
        which is representative of the TaskGraph.
        """
        tasks = {}
        edges = set()
        for key, value in tasks_dict.items():
            tasks[key] = Task.from_json(value)
            if "task_id" in value:
                tasks[key].task_id = value["task_id"]
            for depname, dep in value["dependencies"].items():
                edges.add((key, dep, depname))
        task_graph = cls(tasks, Graph(set(tasks), edges))
        return tasks, task_graph
