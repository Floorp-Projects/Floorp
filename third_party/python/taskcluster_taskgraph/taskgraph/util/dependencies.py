# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from typing import Dict, Iterator, Optional

from taskgraph.task import Task
from taskgraph.transforms.base import TransformConfig
from taskgraph.util.schema import Schema

# Define a collection of group_by functions
GROUP_BY_MAP = {}


def group_by(name, schema=None):
    def wrapper(func):
        assert (
            name not in GROUP_BY_MAP
        ), f"duplicate group_by function name {name} ({func} and {GROUP_BY_MAP[name]})"
        GROUP_BY_MAP[name] = func
        func.schema = schema
        return func

    return wrapper


@group_by("single")
def group_by_single(config, tasks):
    for task in tasks:
        yield [task]


@group_by("all")
def group_by_all(config, tasks):
    return [[task for task in tasks]]


@group_by("attribute", schema=Schema(str))
def group_by_attribute(config, tasks, attr):
    groups = {}
    for task in tasks:
        val = task.attributes.get(attr)
        if not val:
            continue
        groups.setdefault(val, []).append(task)

    return groups.values()


def get_dependencies(config: TransformConfig, task: Dict) -> Iterator[Task]:
    """Iterate over all dependencies as ``Task`` objects.

    Args:
        config (TransformConfig): The ``TransformConfig`` object associated
            with the kind.
        task (Dict): The task dictionary to retrieve dependencies from.

    Returns:
        Iterator[Task]: Returns a generator that iterates over the ``Task``
        objects associated with each dependency.
    """
    if "dependencies" not in task:
        return []

    for label, dep in config.kind_dependencies_tasks.items():
        if label in task["dependencies"].values():
            yield dep


def get_primary_dependency(config: TransformConfig, task: Dict) -> Optional[Task]:
    """Return the ``Task`` object associated with the primary dependency.

    This uses the task's ``primary-kind-dependency`` attribute to find the primary
    dependency, or returns ``None`` if the attribute is unset.

    Args:
        config (TransformConfig): The ``TransformConfig`` object associated
            with the kind.
        task (Dict): The task dictionary to retrieve the primary dependency from.

    Returns:
        Optional[Task]: The ``Task`` object associated with the
            primary dependency or ``None``.
    """
    try:
        primary_kind = task["attributes"]["primary-kind-dependency"]
    except KeyError:
        return None

    for dep in get_dependencies(config, task):
        if dep.kind == primary_kind:
            return dep
