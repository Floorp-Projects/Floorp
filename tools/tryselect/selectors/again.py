# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import os

from ..cli import BaseTryParser
from ..push import push_to_try, history_path


class AgainParser(BaseTryParser):
    name = "again"
    arguments = [
        [
            ["--index"],
            {
                "default": 0,
                "const": "list",
                "nargs": "?",
                "help": "Index of entry in the history to re-push, "
                "where '0' is the most recent (default 0). "
                "Use --index without a value to display indices.",
            },
        ],
        [
            ["--list"],
            {
                "default": False,
                "action": "store_true",
                "dest": "list_configs",
                "help": "Display history and exit",
            },
        ],
        [
            ["--list-tasks"],
            {
                "default": 0,
                "action": "count",
                "dest": "list_tasks",
                "help": "Like --list, but display selected tasks  "
                "for each history entry, up to 10. Repeat "
                "to display all selected tasks.",
            },
        ],
        [
            ["--purge"],
            {
                "default": False,
                "action": "store_true",
                "help": "Remove all history and exit",
            },
        ],
    ]
    common_groups = ["push"]


def run(
    index=0, purge=False, list_configs=False, list_tasks=0, message="{msg}", **pushargs
):
    if index == "list":
        list_configs = True
    else:
        try:
            index = int(index)
        except ValueError:
            print("error: '--index' must be an integer")
            return 1

    if purge:
        os.remove(history_path)
        return

    if not os.path.isfile(history_path):
        print("error: history file not found: {}".format(history_path))
        return 1

    with open(history_path) as fh:
        history = fh.readlines()

    if list_configs or list_tasks > 0:
        for i, data in enumerate(history):
            msg, config = json.loads(data)
            version = config.get("version", "1")
            settings = {}
            if version == 1:
                tasks = config["tasks"]
                settings = config
            elif version == 2:
                try_config = config.get("parameters", {}).get("try_task_config", {})
                tasks = try_config.get("tasks")
            else:
                tasks = None

            if tasks is not None:
                # Select only the things that are of interest to display.
                settings = settings.copy()
                env = settings.pop("env", {}).copy()
                env.pop("TRY_SELECTOR", None)
                for name in ("tasks", "version"):
                    settings.pop(name, None)

                def pluralize(n, noun):
                    return "{n} {noun}{s}".format(
                        n=n, noun=noun, s="" if n == 1 else "s"
                    )

                out = str(i) + ". (" + pluralize(len(tasks), "task")
                if env:
                    out += ", " + pluralize(len(env), "env var")
                if settings:
                    out += ", " + pluralize(len(settings), "setting")
                out += ") " + msg
                print(out)

                if list_tasks > 0:
                    indent = " " * 4
                    if list_tasks > 1:
                        shown_tasks = tasks
                    else:
                        shown_tasks = tasks[:10]
                    print(indent + ("\n" + indent).join(shown_tasks))

                    num_hidden_tasks = len(tasks) - len(shown_tasks)
                    if num_hidden_tasks > 0:
                        print("{}... and {} more".format(indent, num_hidden_tasks))

                if list_tasks and env:
                    for line in ("env: " + json.dumps(env, indent=2)).splitlines():
                        print("    " + line)

                if list_tasks and settings:
                    for line in (
                        "settings: " + json.dumps(settings, indent=2)
                    ).splitlines():
                        print("    " + line)
            else:
                print(
                    "{index}. {msg}".format(
                        index=i,
                        msg=msg,
                    )
                )

        return

    msg, try_task_config = json.loads(history[index])
    return push_to_try(
        "again", message.format(msg=msg), try_task_config=try_task_config, **pushargs
    )
