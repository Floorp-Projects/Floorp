# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import abc


class Task(object):
    """
    Representation of a task in a TaskGraph.  Each Task has, at creation:

    - kind: the name of the task kind
    - label; the label for this task
    - attributes: a dictionary of attributes for this task (used for filtering)
    - task: the task definition (JSON-able dictionary)
    - optimizations: optimizations to apply to the task (see taskgraph.optimize)
    - dependencies: tasks this one depends on, in the form {name: label}, for example
      {'build': 'build-linux64/opt', 'docker-image': 'build-docker-image-desktop-test'}

    And later, as the task-graph processing proceeds:

    - task_id -- TaskCluster taskId under which this task will be created
    - optimized -- true if this task need not be performed

    A kind represents a collection of tasks that share common characteristics.
    For example, all build jobs.  Each instance of a kind is intialized with a
    path from which it draws its task configuration.  The instance is free to
    store as much local state as it needs.
    """
    __metaclass__ = abc.ABCMeta

    def __init__(self, kind, label, attributes, task,
                 optimizations=None, dependencies=None):
        self.kind = kind
        self.label = label
        self.attributes = attributes
        self.task = task

        self.task_id = None
        self.optimized = False

        self.attributes['kind'] = kind

        self.optimizations = optimizations or []
        self.dependencies = dependencies or {}

    def __eq__(self, other):
        return self.kind == other.kind and \
            self.label == other.label and \
            self.attributes == other.attributes and \
            self.task == other.task and \
            self.task_id == other.task_id and \
            self.optimizations == other.optimizations and \
            self.dependencies == other.dependencies

    @classmethod
    def from_json(cls, task_dict):
        """
        Given a data structure as produced by taskgraph.to_json, re-construct
        the original Task object.  This is used to "resume" the task-graph
        generation process, for example in Action tasks.
        """
        return cls(
            kind=task_dict['attributes']['kind'],
            label=task_dict['label'],
            attributes=task_dict['attributes'],
            task=task_dict['task'])
