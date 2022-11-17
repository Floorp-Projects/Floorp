# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import copy

from voluptuous import Required

from taskgraph.task import Task
from taskgraph.util.attributes import sorted_unique_list
from taskgraph.util.schema import Schema

from . import group_tasks


schema = Schema({
    Required('primary-dependency', 'primary dependency task'): Task,
    Required(
        'dependent-tasks',
        'dictionary of dependent tasks, keyed by kind',
    ): {str: Task},
})



def loader(kind, path, config, params, loaded_tasks):
    """
    Load tasks based on the tasks dependent kinds, designed for use as
    multiple-dependent needs.

    Required ``group-by-fn`` is used to define how we coalesce the
    multiple deps together to pass to transforms, e.g. all kinds specified get
    collapsed by platform with `platform`

    Optional ``primary-dependency`` (ordered list or string) is used to determine
    which upstream kind to inherit attrs from. See ``get_primary_dep``.

    The `only-for-build-type` kind configuration, if specified, will limit
    the build types for which a task will be created.

    Optional ``task-template`` kind configuration value, if specified, will be used to
    pass configuration down to the specified transforms used.
    """
    task_template = config.get('task-template')

    for dep_tasks in group_tasks(config, loaded_tasks):
        kinds = [dep.kind for dep in dep_tasks]
        assert_unique_members(
            kinds, error_msg="multi_dep.py should have filtered down to one task per kind"
        )

        dep_tasks_per_kind = {dep.kind: dep for dep in dep_tasks}

        task = {'dependent-tasks': dep_tasks_per_kind}
        task['primary-dependency'] = get_primary_dep(config, dep_tasks_per_kind)
        if task_template:
            task.update(copy.deepcopy(task_template))

        yield task


def assert_unique_members(kinds, error_msg=None):
    if len(kinds) != len(set(kinds)):
        raise Exception(error_msg)


def get_primary_dep(config, dep_tasks):
    """Find the dependent task to inherit attributes from.

    If ``primary-dependency`` is defined in ``kind.yml`` and is a string,
    then find the first dep with that task kind and return it. If it is
    defined and is a list, the first kind in that list with a matching dep
    is the primary dependency. If it's undefined, return the first dep.

    """
    primary_dependencies = config.get('primary-dependency')
    if isinstance(primary_dependencies, str):
        primary_dependencies = [primary_dependencies]
    if not primary_dependencies:
        assert len(dep_tasks) == 1, "Must define a primary-dependency!"
        return dep_tasks.values()[0]
    primary_dep = None
    for primary_kind in primary_dependencies:
        for dep_kind in dep_tasks:
            if dep_kind == primary_kind:
                assert primary_dep is None, \
                    "Too many primary dependent tasks in dep_tasks: {}!".format(
                        [t.label for t in dep_tasks]
                    )
                primary_dep = dep_tasks[dep_kind]
    if primary_dep is None:
        raise Exception("Can't find dependency of {}: {}".format(
            config['primary-dependency'], config
        ))
    return primary_dep
