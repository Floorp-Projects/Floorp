# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.task import Task
from taskgraph.util.readonlydict import ReadOnlyDict

immutable_types = {int, float, bool, str, type(None), ReadOnlyDict}


def copy_task(obj):
    """
    Perform a deep copy of a task that has a tree-like structure.

    Unlike copy.deepcopy, this does *not* support copying graph-like structure,
    but it does it more efficiently than deepcopy.
    """
    ty = type(obj)
    if ty in immutable_types:
        return obj
    if ty is dict:
        return {k: copy_task(v) for k, v in obj.items()}
    if ty is list:
        return [copy_task(elt) for elt in obj]
    if ty is Task:
        task = Task(
            kind=copy_task(obj.kind),
            label=copy_task(obj.label),
            attributes=copy_task(obj.attributes),
            task=copy_task(obj.task),
            description=copy_task(obj.description),
            optimization=copy_task(obj.optimization),
            dependencies=copy_task(obj.dependencies),
            soft_dependencies=copy_task(obj.soft_dependencies),
            if_dependencies=copy_task(obj.if_dependencies),
        )
        if obj.task_id:
            task.task_id = obj.task_id
        return task
    raise NotImplementedError(f"copying '{ty}' from '{obj}'")
