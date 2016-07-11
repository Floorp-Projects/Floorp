# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

from .graph import Graph
from .util.python_path import find_object

TASKCLUSTER_QUEUE_URL = "https://queue.taskcluster.net/v1/task/"
GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..'))


class TaskGraph(object):
    """
    Representation of a task graph.

    A task graph is a combination of a Graph and a dictionary of tasks indexed
    by label.  TaskGraph instances should be treated as immutable.
    """

    def __init__(self, tasks, graph):
        assert set(tasks) == graph.nodes
        self.tasks = tasks
        self.graph = graph

    def to_json(self):
        "Return a JSON-able object representing the task graph, as documented"
        named_links_dict = self.graph.named_links_dict()
        # this dictionary may be keyed by label or by taskid, so let's just call it 'key'
        tasks = {}
        for key in self.graph.visit_postorder():
            task = self.tasks[key]
            implementation = task.__class__.__module__ + ":" + task.__class__.__name__
            task_json = {
                'label': task.label,
                'attributes': task.attributes,
                'dependencies': named_links_dict.get(key, {}),
                'task': task.task,
                'kind_implementation': implementation
            }
            if task.task_id:
                task_json['task_id'] = task.task_id
            tasks[key] = task_json
        return tasks

    def __getitem__(self, label):
        "Get a task by label"
        return self.tasks[label]

    def __iter__(self):
        "Iterate over tasks in undefined order"
        return self.tasks.itervalues()

    def __repr__(self):
        return "<TaskGraph graph={!r} tasks={!r}>".format(self.graph, self.tasks)

    def __eq__(self, other):
        return self.tasks == other.tasks and self.graph == other.graph

    @classmethod
    def from_json(cls, tasks_dict, root):
        """
        This code is used to generate the a TaskGraph using a dictionary
        which is representative of the TaskGraph.
        """
        tasks = {}
        edges = set()
        for key, value in tasks_dict.iteritems():
            # We get the implementation from JSON
            implementation = value['kind_implementation']
            # Loading the module and creating a Task from a dictionary
            task_kind = find_object(implementation)
            tasks[key] = task_kind.from_json(value)
            if 'task_id' in value:
                tasks[key].task_id = value['task_id']
            for depname, dep in value['dependencies'].iteritems():
                edges.add((key, dep, depname))
        task_graph = cls(tasks, Graph(set(tasks), edges))
        return tasks, task_graph
