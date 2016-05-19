# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import abc

class Kind(object):
    """
    A kind represents a collection of tasks that share common characteristics.
    For example, all build jobs.  Each instance of a kind is intialized with a
    path from which it draws its task configuration.  The instance is free to
    store as much local state as it needs.
    """
    __metaclass__ = abc.ABCMeta

    def __init__(self, path, config):
        self.name = os.path.basename(path)
        self.path = path
        self.config = config

    @abc.abstractmethod
    def load_tasks(self, parameters):
        """
        Get the set of tasks of this kind.

        The `parameters` give details on which to base the task generation.
        See `taskcluster/docs/parameters.rst` for details.

        The return value is a list of Task instances.
        """

    @abc.abstractmethod
    def get_task_dependencies(self, task, taskgraph):
        """
        Get the set of task labels this task depends on, by querying the task graph.

        Returns a list of (task_label, dependency_name) pairs describing the
        dependencies.
        """

    @abc.abstractmethod
    def get_task_optimization_key(self, task, taskgraph):
        """
        Get the *optimization key* for the given task.  When called, all
        dependencies of this task will already have their `optimization_key`
        attribute set.

        The optimization key is a unique identifier covering all inputs to this
        task.  If another task with the same optimization key has already been
        performed, it will be used directly instead of executing the task
        again.

        Returns a string suitable for inclusion in a TaskCluster index
        namespace (generally of the form `<optimizationName>.<hash>`), or None
        if this task cannot be optimized.
        """

    @abc.abstractmethod
    def get_task_definition(self, task, dependent_taskids):
        """
        Get the final task definition for the given task.  This is the time to
        substitute actual taskIds for dependent tasks into the task definition.
        Note that this method is only used in the decision tasks, so it should
        not perform any processing that users might want to test or see in
        other `mach taskgraph` commands.

        The `dependent_taskids` parameter is a dictionary mapping dependency
        name to assigned taskId.

        The returned task definition will be modified before being passed to
        `queue.createTask`.
        """
