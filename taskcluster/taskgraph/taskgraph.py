# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals


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
            task_json = {
                'label': task.label,
                'attributes': task.attributes,
                'dependencies': named_links_dict.get(key, {}),
                'task': task.task
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
