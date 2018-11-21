# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import attr


@attr.s
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

    kind = attr.ib()
    label = attr.ib()
    attributes = attr.ib()
    task = attr.ib()
    task_id = attr.ib(default=None, init=False)
    optimization = attr.ib(default=None)
    dependencies = attr.ib(factory=dict)
    release_artifacts = attr.ib(
        converter=attr.converters.optional(frozenset),
        default=None,
    )

    def __attrs_post_init__(self):
        self.attributes['kind'] = self.kind

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
        if self.release_artifacts:
            rv['release_artifacts'] = sorted(self.release_artifacts)
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
            dependencies=task_dict.get('dependencies'),
            release_artifacts=task_dict.get('release-artifacts'),
        )
        if 'task_id' in task_dict:
            rv.task_id = task_dict['task_id']
        return rv
