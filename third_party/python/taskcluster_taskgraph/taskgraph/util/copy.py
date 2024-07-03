from typing import Any

from taskgraph.task import Task
from taskgraph.util.readonlydict import ReadOnlyDict

immutable_types = {int, float, bool, str, type(None), ReadOnlyDict}


def deepcopy(obj: Any) -> Any:
    """Perform a deep copy of an object with a tree like structure.

    This is a re-implementation of Python's `copy.deepcopy` function with a few key differences:

    1. Unlike the stdlib, this does *not* support copying graph-like structure,
    which allows it to be more efficient than deepcopy on tree-like structures
    (such as Tasks).
    2. This special cases support for `taskgraph.task.Task` objects.

    Args:
        obj: The object to deep copy.

    Returns:
        A deep copy of the object.
    """
    ty = type(obj)
    if ty in immutable_types:
        return obj
    if ty is dict:
        return {k: deepcopy(v) for k, v in obj.items()}
    if ty is list:
        return [deepcopy(elt) for elt in obj]
    if ty is Task:
        task = Task(
            kind=deepcopy(obj.kind),
            label=deepcopy(obj.label),
            attributes=deepcopy(obj.attributes),
            task=deepcopy(obj.task),
            description=deepcopy(obj.description),
            optimization=deepcopy(obj.optimization),
            dependencies=deepcopy(obj.dependencies),
            soft_dependencies=deepcopy(obj.soft_dependencies),
            if_dependencies=deepcopy(obj.if_dependencies),
        )
        if obj.task_id:
            task.task_id = obj.task_id
        return task
    raise NotImplementedError(f"copying '{ty}' from '{obj}'")
