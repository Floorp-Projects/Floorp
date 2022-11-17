# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import copy

from voluptuous import Required

from taskgraph.task import Task
from taskgraph.util.schema import Schema

from . import group_tasks

schema = Schema({
    Required(
        'dependent-tasks',
        'dictionary of dependent tasks, keyed by kind',
    ): {str: Task},
})


def loader(kind, path, config, params, loaded_tasks):
    """
    Load tasks based on the tasks dependent kind, designed to attach all dependencies of a single
    kind to the loaded task.

    Required ``group-by-fn`` is used to define how we coalesce the
    multiple deps together to pass to transforms, e.g. all kinds specified get
    collapsed by platform with `build-type`

    Optional ``task-template`` kind configuration value, if specified, will be used to
    pass configuration down to the specified transforms used.
    """
    task_template = config.get('task-template')

    for dep_tasks in group_tasks(config, loaded_tasks):
        task = {'dependent-tasks': {dep.label: dep for dep in dep_tasks}}
        if task_template:
            task.update(copy.deepcopy(task_template))

        yield task
