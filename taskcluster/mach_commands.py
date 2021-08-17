# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals
import argparse
import json
import logging
import os
import shlex
import subprocess
import sys
import tempfile
import time
import traceback
from functools import partial

from mach.decorators import (
    Command,
    CommandArgument,
    CommandProvider,
    SettingsProvider,
    SubCommand,
)
from mozbuild.base import MachCommandBase

from taskgraph.main import (
    commands as taskgraph_commands,
    format_taskgraph,
)

logger = logging.getLogger("taskcluster")


@SettingsProvider
class TaskgraphConfig(object):
    @classmethod
    def config_settings(cls):
        return [
            (
                "taskgraph.diffcmd",
                "string",
                "The command to run with `./mach taskgraph --diff`",
                "diff --report-identical-files --color=always "
                "--label={attr}@{base} --label={attr}@{cur} -U20",
                {},
            )
        ]


def strtobool(value):
    """Convert string to boolean.

    Wraps "distutils.util.strtobool", deferring the import of the package
    in case it's not installed. Otherwise, we have a "chicken and egg problem" where
    |mach bootstrap| would install the required package to enable "distutils.util", but
    it can't because mach fails to interpret this file.
    """
    from distutils.util import strtobool

    return bool(strtobool(value))


class ShowTaskGraphSubCommand(SubCommand):
    """A SubCommand with TaskGraph-specific arguments"""

    def __call__(self, func):
        name = func.__name__.replace("_", "-").split("-", 1)[1]
        args = taskgraph_commands[name].func.args

        extra_args = [
            (
                ["--diff"],
                {
                    "const": "default",
                    "nargs": "?",
                    "default": None,
                    "help": "Generate and diff the current taskgraph against another revision. "
                    "Without args the base revision will be used. A revision specifier such as "
                    "the hash or `.~1` (hg) or `HEAD~1` (git) can be used as well.",
                },
            ),
        ]
        extra_args.reverse()  # ensures args displayed in same order they're defined

        after = SubCommand.__call__(self, func)
        for arg in extra_args + args:
            arg = CommandArgument(*arg[0], **arg[1])
            after = arg(after)
        return after


def get_taskgraph_command_parser(name):
    """Given a command name, obtain its argument parser.

    Args:
        name (str): Name of the command.

    Returns:
        ArgumentParser: An ArgumentParser instance.
    """
    command = taskgraph_commands[name]
    parser = argparse.ArgumentParser()
    for arg in command.func.args:
        parser.add_argument(*arg[0], **arg[1])

    return parser


def get_taskgraph_decision_parser():
    parser = get_taskgraph_command_parser("decision")

    extra_args = [
        (
            ["--optimize-target-tasks"],
            {
                "type": lambda flag: strtobool(flag),
                "nargs": "?",
                "const": "true",
                "help": "If specified, this indicates whether the target "
                "tasks are eligible for optimization. Otherwise, the default "
                "for the project is used.",
            },
        ),
        (
            ["--include-push-tasks"],
            {
                "action": "store_true",
                "help": "Whether tasks from the on-push graph should be re-used "
                "in this graph. This allows cron graphs to avoid rebuilding "
                "jobs that were built on-push.",
            },
        ),
        (
            ["--rebuild-kind"],
            {
                "dest": "rebuild_kinds",
                "action": "append",
                "default": argparse.SUPPRESS,
                "help": "Kinds that should not be re-used from the on-push graph.",
            },
        ),
        (
            ["--comm-base-repository"],
            {
                "required": False,
                "help": "URL for 'base' comm-* repository to clone",
            },
        ),
        (
            ["--comm-head-repository"],
            {
                "required": False,
                "help": "URL for 'head' comm-* repository to fetch revision from",
            },
        ),
        (
            ["--comm-head-ref"],
            {
                "required": False,
                "help": "comm-* Reference (this is same as rev usually for hg)",
            },
        ),
        (
            ["--comm-head-rev"],
            {
                "required": False,
                "help": "Commit revision to use from head comm-* repository",
            },
        ),
    ]
    for arg in extra_args:
        parser.add_argument(*arg[0], **arg[1])

    return parser


@CommandProvider
class MachCommands(MachCommandBase):
    @Command(
        "taskgraph",
        category="ci",
        description="Manipulate TaskCluster task graphs defined in-tree",
    )
    def taskgraph(self, command_context):
        """The taskgraph subcommands all relate to the generation of task graphs
        for Gecko continuous integration.  A task graph is a set of tasks linked
        by dependencies: for example, a binary must be built before it is tested,
        and that build may further depend on various toolchains, libraries, etc.
        """

    @ShowTaskGraphSubCommand(
        "taskgraph", "tasks", description="Show all tasks in the taskgraph"
    )
    def taskgraph_tasks(self, command_context, **options):
        options["graph_attr"] = "full_task_set"
        return self.show_taskgraph(command_context, options)

    @ShowTaskGraphSubCommand("taskgraph", "full", description="Show the full taskgraph")
    def taskgraph_full(self, command_context, **options):
        options["graph_attr"] = "full_task_graph"
        return self.show_taskgraph(command_context, options)

    @ShowTaskGraphSubCommand(
        "taskgraph", "target", description="Show the target task set"
    )
    def taskgraph_target(self, command_context, **options):
        options["graph_attr"] = "target_task_set"
        return self.show_taskgraph(command_context, options)

    @ShowTaskGraphSubCommand(
        "taskgraph", "target-graph", description="Show the target taskgraph"
    )
    def taskgraph_target_graph(self, command_context, **options):
        options["graph_attr"] = "target_task_graph"
        return self.show_taskgraph(command_context, options)

    @ShowTaskGraphSubCommand(
        "taskgraph", "optimized", description="Show the optimized taskgraph"
    )
    def taskgraph_optimized(self, command_context, **options):
        options["graph_attr"] = "optimized_task_graph"
        return self.show_taskgraph(command_context, options)

    @ShowTaskGraphSubCommand(
        "taskgraph", "morphed", description="Show the morphed taskgraph"
    )
    def taskgraph_morphed(self, command_context, **options):
        options["graph_attr"] = "morphed_task_graph"
        return self.show_taskgraph(command_context, options)

    @SubCommand("taskgraph", "actions", description="Write actions.json to stdout")
    @CommandArgument(
        "--root", "-r", help="root of the taskgraph definition relative to topsrcdir"
    )
    @CommandArgument(
        "--quiet", "-q", action="store_true", help="suppress all logging output"
    )
    @CommandArgument(
        "--verbose",
        "-v",
        action="store_true",
        help="include debug-level logging output",
    )
    @CommandArgument(
        "--parameters",
        "-p",
        default="project=mozilla-central",
        help="parameters file (.yml or .json; see "
        "`taskcluster/docs/parameters.rst`)`",
    )
    def taskgraph_actions(self, command_context, **options):
        return self.show_actions(command_context, options)

    @SubCommand(
        "taskgraph",
        "decision",
        description="Run the decision task",
        parser=get_taskgraph_decision_parser,
    )
    def taskgraph_decision(self, command_context, **options):
        """Run the decision task: generate a task graph and submit to
        TaskCluster.  This is only meant to be called within decision tasks,
        and requires a great many arguments.  Commands like `mach taskgraph
        optimized` are better suited to use on the command line, and can take
        the parameters file generated by a decision task."""
        try:
            self.setup_logging(command_context)
            start = time.monotonic()
            ret = taskgraph_commands["decision"].func(options)
            end = time.monotonic()
            if os.environ.get("MOZ_AUTOMATION") == "1":
                perfherder_data = {
                    "framework": {"name": "build_metrics"},
                    "suites": [
                        {
                            "name": "decision",
                            "value": end - start,
                            "lowerIsBetter": True,
                            "shouldAlert": True,
                            "subtests": [],
                        }
                    ],
                }
                print(
                    "PERFHERDER_DATA: {}".format(json.dumps(perfherder_data)),
                    file=sys.stderr,
                )
            return ret
        except Exception:
            traceback.print_exc()
            sys.exit(1)

    @SubCommand(
        "taskgraph",
        "cron",
        description="Provide a pointer to the new `.cron.yml` handler.",
    )
    def taskgraph_cron(self, command_context, **options):
        print(
            'Handling of ".cron.yml" files has move to '
            "https://hg.mozilla.org/ci/ci-admin/file/default/build-decision."
        )
        sys.exit(1)

    @SubCommand(
        "taskgraph",
        "action-callback",
        description="Run action callback used by action tasks",
        parser=partial(get_taskgraph_command_parser, "action-callback"),
    )
    def action_callback(self, command_context, **options):
        self.setup_logging(command_context)
        taskgraph_commands["action-callback"].func(options)

    @SubCommand(
        "taskgraph",
        "test-action-callback",
        description="Run an action callback in a testing mode",
        parser=partial(get_taskgraph_command_parser, "test-action-callback"),
    )
    def test_action_callback(self, command_context, **options):
        self.setup_logging(command_context)

        if not options["parameters"]:
            options["parameters"] = "project=mozilla-central"

        taskgraph_commands["test-action-callback"].func(options)

    def setup_logging(self, command_context, quiet=False, verbose=True):
        """
        Set up Python logging for all loggers, sending results to stderr (so
        that command output can be redirected easily) and adding the typical
        mach timestamp.
        """
        # remove the old terminal handler
        old = command_context.log_manager.replace_terminal_handler(None)

        # re-add it, with level and fh set appropriately
        if not quiet:
            level = logging.DEBUG if verbose else logging.INFO
            command_context.log_manager.add_terminal_logging(
                fh=sys.stderr,
                level=level,
                write_interval=old.formatter.write_interval,
                write_times=old.formatter.write_times,
            )

        # all of the taskgraph logging is unstructured logging
        command_context.log_manager.enable_unstructured()

    def show_taskgraph(self, command_context, options):
        self.setup_logging(
            command_context, quiet=options["quiet"], verbose=options["verbose"]
        )
        vcs = None
        base_out = ""
        base_ref = None
        cur_ref = None

        if not options["parameters"]:
            options["parameters"] = "project=mozilla-central"

        if options["diff"]:
            from mozversioncontrol import get_repository_object

            vcs = get_repository_object(command_context.topsrcdir)
            with vcs:
                if not vcs.working_directory_clean():
                    print("abort: can't diff taskgraph with dirty working directory")
                    return 1

                # We want to return the working directory to the current state
                # as best we can after we're done. In all known cases, using
                # branch or bookmark (which are both available on the VCS object)
                # as `branch` is preferable to a specific revision.
                cur_ref = vcs.branch or vcs.head_ref[:12]
            logger.info("Generating {} @ {}".format(options["graph_attr"], cur_ref))

        out = format_taskgraph(options)

        if options["diff"]:
            with vcs:
                # Some transforms use global state for checks, so will fail
                # when running taskgraph a second time in the same session.
                # Reload all taskgraph modules to avoid this.
                for mod in sys.modules.copy():
                    if mod.startswith("taskgraph"):
                        del sys.modules[mod]

                if options["diff"] == "default":
                    base_ref = vcs.base_ref
                else:
                    base_ref = options["diff"]

                try:
                    vcs.update(base_ref)
                    base_ref = vcs.head_ref[:12]
                    logger.info(
                        "Generating {} @ {}".format(options["graph_attr"], base_ref)
                    )
                    base_out = format_taskgraph(options)
                finally:
                    vcs.update(cur_ref)

            diffcmd = command_context._mach_context.settings["taskgraph"]["diffcmd"]
            diffcmd = diffcmd.format(
                attr=options["graph_attr"], base=base_ref, cur=cur_ref
            )

            with tempfile.NamedTemporaryFile(mode="w") as base:
                base.write(base_out)

                with tempfile.NamedTemporaryFile(mode="w") as cur:
                    cur.write(out)
                    try:
                        out = subprocess.run(
                            shlex.split(diffcmd)
                            + [
                                base.name,
                                cur.name,
                            ],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            universal_newlines=True,
                            check=True,
                        ).stdout
                    except subprocess.CalledProcessError as e:
                        # returncode 1 simply means diffs were found
                        if e.returncode != 1:
                            print(e.stderr, file=sys.stderr)
                            raise
                        out = e.output

        fh = options["output_file"]
        if fh:
            fh = open(fh, "w")

        print(out, file=fh)

        if options["diff"] and options["format"] != "json":
            logger.info(
                "If you were expecting differences in task bodies "
                'you should pass "-J"\n'
            )

    def show_actions(self, command_context, options):
        import taskgraph
        import taskgraph.actions
        import taskgraph.generator
        import taskgraph.parameters

        try:
            self.setup_logging(
                command_context, quiet=options["quiet"], verbose=options["verbose"]
            )
            parameters = taskgraph.parameters.parameters_loader(options["parameters"])

            tgg = taskgraph.generator.TaskGraphGenerator(
                root_dir=options.get("root"),
                parameters=parameters,
            )

            actions = taskgraph.actions.render_actions_json(
                tgg.parameters,
                tgg.graph_config,
                decision_task_id="DECISION-TASK",
            )
            print(json.dumps(actions, sort_keys=True, indent=2, separators=(",", ": ")))
        except Exception:
            traceback.print_exc()
            sys.exit(1)


@CommandProvider
class TaskClusterImagesProvider(MachCommandBase):
    @Command(
        "taskcluster-load-image",
        category="ci",
        description="Load a pre-built Docker image. Note that you need to "
        "have docker installed and running for this to work.",
        parser=partial(get_taskgraph_command_parser, "load-image"),
    )
    def load_image(self, command_context, **kwargs):
        taskgraph_commands["load-image"].func(kwargs)

    @Command(
        "taskcluster-build-image",
        category="ci",
        description="Build a Docker image",
        parser=partial(get_taskgraph_command_parser, "build-image"),
    )
    def build_image(self, command_context, **kwargs):
        try:
            taskgraph_commands["build-image"].func(kwargs)
        except Exception:
            traceback.print_exc()
            sys.exit(1)

    @Command(
        "taskcluster-image-digest",
        category="ci",
        description="Print the digest of the image of this name based on the "
        "current contents of the tree.",
        parser=partial(get_taskgraph_command_parser, "build-image"),
    )
    def image_digest(self, command_context, **kwargs):
        taskgraph_commands["image-digest"].func(kwargs)


@CommandProvider
class TaskClusterPartialsData(MachCommandBase):
    @Command(
        "release-history",
        category="ci",
        description="Query balrog for release history used by enable partials generation",
    )
    @CommandArgument(
        "-b",
        "--branch",
        help="The gecko project branch used in balrog, such as "
        "mozilla-central, release, maple",
    )
    @CommandArgument(
        "--product", default="Firefox", help="The product identifier, such as 'Firefox'"
    )
    def generate_partials_builds(self, command_context, product, branch):
        from taskgraph.util.partials import populate_release_history

        try:
            import yaml

            release_history = {
                "release_history": populate_release_history(product, branch)
            }
            print(
                yaml.safe_dump(
                    release_history, allow_unicode=True, default_flow_style=False
                )
            )
        except Exception:
            traceback.print_exc()
            sys.exit(1)
