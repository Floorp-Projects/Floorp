# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import subprocess
import tempfile
from argparse import ArgumentParser

from .task_config import all_task_configs

COMMON_ARGUMENT_GROUPS = {
    "push": [
        [
            ["-m", "--message"],
            {
                "const": "editor",
                "default": "{msg}",
                "nargs": "?",
                "help": "Use the specified commit message, or create it in your "
                "$EDITOR if blank. Defaults to computed message.",
            },
        ],
        [
            ["--closed-tree"],
            {
                "action": "store_true",
                "default": False,
                "help": "Push despite a closed try tree",
            },
        ],
        [
            ["--push-to-lando"],
            {
                "action": "store_true",
                "default": False,
                "help": "Submit changes for Lando to push to try.",
            },
        ],
    ],
    "preset": [
        [
            ["--save"],
            {
                "default": None,
                "help": "Save selection for future use with --preset.",
            },
        ],
        [
            ["--preset"],
            {
                "default": None,
                "help": "Load a saved selection.",
            },
        ],
        [
            ["--list-presets"],
            {
                "action": "store_const",
                "dest": "preset_action",
                "const": "list",
                "default": None,
                "help": "List available preset selections.",
            },
        ],
        [
            ["--edit-presets"],
            {
                "action": "store_const",
                "dest": "preset_action",
                "const": "edit",
                "default": None,
                "help": "Edit the preset file.",
            },
        ],
    ],
    "task": [
        [
            ["--full"],
            {
                "action": "store_true",
                "default": False,
                "help": "Use the full set of tasks as input to fzf (instead of "
                "target tasks).",
            },
        ],
        [
            ["-p", "--parameters"],
            {
                "default": None,
                "help": "Use the given parameters.yml to generate tasks, "
                "defaults to a default set of parameters",
            },
        ],
    ],
}

NO_PUSH_ARGUMENT_GROUP = [
    [
        ["--stage-changes"],
        {
            "dest": "stage_changes",
            "action": "store_true",
            "help": "Locally stage changes created by this command but do not "
            "push to try.",
        },
    ],
    [
        ["--no-push"],
        {
            "dest": "dry_run",
            "action": "store_true",
            "help": "Do not push to try as a result of running this command (if "
            "specified this command will only print calculated try "
            "syntax and selection info and not change files).",
        },
    ],
]


class BaseTryParser(ArgumentParser):
    name = "try"
    common_groups = ["push", "preset"]
    arguments = []
    task_configs = []

    def __init__(self, *args, **kwargs):
        ArgumentParser.__init__(self, *args, **kwargs)

        group = self.add_argument_group("{} arguments".format(self.name))
        for cli, kwargs in self.arguments:
            group.add_argument(*cli, **kwargs)

        for name in self.common_groups:
            group = self.add_argument_group("{} arguments".format(name))
            arguments = COMMON_ARGUMENT_GROUPS[name]

            # Preset arguments are all mutually exclusive.
            if name == "preset":
                group = group.add_mutually_exclusive_group()

            for cli, kwargs in arguments:
                group.add_argument(*cli, **kwargs)

            if name == "push":
                group_no_push = group.add_mutually_exclusive_group()
                arguments = NO_PUSH_ARGUMENT_GROUP
                for cli, kwargs in arguments:
                    group_no_push.add_argument(*cli, **kwargs)

        group = self.add_argument_group("task configuration arguments")
        self.task_configs = {c: all_task_configs[c]() for c in self.task_configs}
        for cfg in self.task_configs.values():
            cfg.add_arguments(group)

    def validate(self, args):
        if hasattr(args, "message"):
            if args.message == "editor":
                if "EDITOR" not in os.environ:
                    self.error(
                        "must set the $EDITOR environment variable to use blank --message"
                    )

                with tempfile.NamedTemporaryFile(mode="r") as fh:
                    subprocess.call([os.environ["EDITOR"], fh.name])
                    args.message = fh.read().strip()

            if "{msg}" not in args.message:
                args.message = "{}\n\n{}".format(args.message, "{msg}")

    def parse_known_args(self, *args, **kwargs):
        args, remainder = ArgumentParser.parse_known_args(self, *args, **kwargs)
        self.validate(args)
        return args, remainder
