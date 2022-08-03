# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging

from taskgraph.util.templates import merge
from taskgraph.util.yaml import load_yaml

logger = logging.getLogger(__name__)


def loader(kind, path, config, params, loaded_tasks):
    """
    Get the input elements that will be transformed into tasks in a generic
    way.  The elements themselves are free-form, and become the input to the
    first transform.

    By default, this reads tasks from the `tasks` key, or from yaml files
    named by `tasks-from`. The entities are read from mappings, and the
    keys to those mappings are added in the `name` key of each entity.

    If there is a `task-defaults` config, then every task is merged with it.
    This provides a simple way to set default values for all tasks of a kind.
    The `task-defaults` key can also be specified in a yaml file pointed to by
    `tasks-from`. In this case it will only apply to tasks defined in the same
    file.

    Other kind implementations can use a different loader function to
    produce inputs and hand them to `transform_inputs`.
    """

    def generate_tasks():
        defaults = config.get("task-defaults")
        for name, task in config.get("tasks", {}).items():
            if defaults:
                task = merge(defaults, task)
            task["task-from"] = "kind.yml"
            yield name, task

        for filename in config.get("tasks-from", []):
            tasks = load_yaml(path, filename)

            file_defaults = tasks.pop("task-defaults", None)
            if defaults:
                file_defaults = merge(defaults, file_defaults or {})

            for name, task in tasks.items():
                if file_defaults:
                    task = merge(file_defaults, task)
                task["task-from"] = filename
                yield name, task

    for name, task in generate_tasks():
        task["name"] = name
        logger.debug(f"Generating tasks for {kind} {name}")
        yield task
