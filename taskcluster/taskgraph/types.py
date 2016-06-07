# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

class Task(object):
    """
    Representation of a task in a TaskGraph.

    Each has, at creation:

    - kind: Kind instance that created this task
    - label; the label for this task
    - attributes: a dictionary of attributes for this task (used for filtering)
    - task: the task definition (JSON-able dictionary)
    - extra: extra kind-specific metadata

    And later, as the task-graph processing proceeds:

    - optimization_key -- key for finding equivalent tasks in the TC index
    - task_id -- TC taskId under which this task will be created
    """

    def __init__(self, kind, label, attributes=None, task=None, **extra):
        self.kind = kind
        self.label = label
        self.attributes = attributes or {}
        self.task = task or {}
        self.extra = extra

        self.task_id = None

        if not (all(isinstance(x, basestring) for x in self.attributes.iterkeys()) and
                all(isinstance(x, basestring) for x in self.attributes.itervalues())):
            raise TypeError("attribute names and values must be strings")

    def __str__(self):
        return "{} ({})".format(self.task_id or self.label,
                                self.task['metadata']['description'].strip())


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
