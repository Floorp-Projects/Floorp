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

    def optimize_task(self, task):
        """
        Determine whether this task can be optimized, and if it can, what taskId
        it should be replaced with.

        The return value is a tuple `(optimized, taskId)`.  If `optimized` is
        true, then the task will be optimized (in other words, not included in
        the task graph).  If the second argument is a taskid, then any
        dependencies on this task will isntead depend on that taskId.  It is an
        error to return no taskId for a task on which other tasks depend.

        The default never optimizes.
        """
        return False, None
