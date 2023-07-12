# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_dependencies, get_primary_dependency
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.treeherder import inherit_treeherder_from_dep, join_symbol

transforms = TransformSequence()


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        if not task["attributes"].get("build-type"):
            task["attributes"]["build-type"] = task["name"]

        resolve_keyed_by(
            task,
            "treeherder.symbol",
            item_name=task["name"],
            **{
                "build-type": task["attributes"]["build-type"],
                "level": config.params["level"],
            },
        )
        yield task


@transforms.add
def build_treeherder_definition(config, tasks):
    for task in tasks:
        primary_dep = get_primary_dependency(config, task)
        if not primary_dep and task.get("primary-dependency"):
            primary_dep = task.pop("primary-dependency")

        elif not primary_dep:
            deps = list(get_dependencies(config, task)) or list(
                task["dependent-tasks"].values()
            )
            primary_dep = deps[0]

        task.setdefault("treeherder", {}).update(
            inherit_treeherder_from_dep(task, primary_dep)
        )
        task_group = primary_dep.task["extra"]["treeherder"].get("groupSymbol", "?")
        job_symbol = task["treeherder"].pop("symbol")
        full_symbol = join_symbol(task_group, job_symbol)
        task["treeherder"]["symbol"] = full_symbol

        yield task
