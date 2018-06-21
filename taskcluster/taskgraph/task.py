# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals


class Task(object):
    """
    Representation of a task in a TaskGraph.  Each Task has, at creation:

    - kind: the name of the task kind
    - label; the label for this task
    - attributes: a dictionary of attributes for this task (used for filtering)
    - task: the task definition (JSON-able dictionary)
    - optimization: optimization to apply to the task (see taskgraph.optimize)
    - dependencies: tasks this one depends on, in the form {name: label}, for example
      {'build': 'build-linux64/opt', 'docker-image': 'build-docker-image-desktop-test'}

    And later, as the task-graph processing proceeds:

    - task_id -- TaskCluster taskId under which this task will be created

    This class is just a convenience wrapper for the data type and managing
    display, comparison, serialization, etc. It has no functionality of its own.
    """
    def __init__(self, kind, label, attributes, task,
                 optimization=None, dependencies=None):
        self.kind = kind
        self.label = label
        self.attributes = attributes
        self.task = task

        self.task_id = None

        self.attributes['kind'] = kind

        self.optimization = optimization
        self.dependencies = dependencies or {}

    def __eq__(self, other):
        return self.kind == other.kind and \
            self.label == other.label and \
            self.attributes == other.attributes and \
            self.task == other.task and \
            self.task_id == other.task_id and \
            self.optimization == other.optimization and \
            self.dependencies == other.dependencies

    def __repr__(self):
        return ('Task({kind!r}, {label!r}, {attributes!r}, {task!r}, '
                'optimization={optimization!r}, '
                'dependencies={dependencies!r})'.format(**self.__dict__))

    def to_json(self):
        rv = {
            'kind': self.kind,
            'label': self.label,
            'attributes': self.attributes,
            'dependencies': self.dependencies,
            'optimization': self.optimization,
            'task': self.task,
        }
        if self.task_id:
            rv['task_id'] = self.task_id
        return rv

    @classmethod
    def from_json(cls, task_dict):
        """
        Given a data structure as produced by taskgraph.to_json, re-construct
        the original Task object.  This is used to "resume" the task-graph
        generation process, for example in Action tasks.
        """
        rv = cls(
            kind=task_dict['kind'],
            label=task_dict['label'],
            attributes=task_dict['attributes'],
            task=task_dict['task'],
            optimization=task_dict['optimization'],
            dependencies=task_dict.get('dependencies'))
        if 'task_id' in task_dict:
            rv.task_id = task_dict['task_id']
        return rv
