# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import argparse
import json
import logging
import os
import sys
import time
import traceback
from functools import partial

import gecko_taskgraph.main
from gecko_taskgraph.main import commands as taskgraph_commands
from mach.decorators import Command, CommandArgument, SubCommand

logger = logging.getLogger("taskcluster")


def strtobool(value):
    """Convert string to boolean.

    Wraps "distutils.util.strtobool", deferring the import of the package
    in case it's not installed. Otherwise, we have a "chicken and egg problem" where
    |mach bootstrap| would install the required package to enable "distutils.util", but
    it can't because mach fails to interpret this file.
    """
    from distutils.util import strtobool

    return bool(strtobool(value))


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

    parser.set_defaults(func=command.func, **command.defaults)
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
    ]
    for arg in extra_args:
        parser.add_argument(*arg[0], **arg[1])

    return parser


@Command(
    "taskgraph",
    category="ci",
    description="Manipulate TaskCluster task graphs defined in-tree",
)
def taskgraph_command(command_context):
    """The taskgraph subcommands all relate to the generation of task graphs
    for Gecko continuous integration.  A task graph is a set of tasks linked
    by dependencies: for example, a binary must be built before it is tested,
    and that build may further depend on various toolchains, libraries, etc.
    """


@SubCommand(
    "taskgraph",
    "tasks",
    description="Show all tasks in the taskgraph",
    parser=partial(get_taskgraph_command_parser, "tasks"),
)
def taskgraph_tasks(command_context, **options):
    return run_show_taskgraph(command_context, **options)


@SubCommand(
    "taskgraph",
    "full",
    description="Show the full taskgraph",
    parser=partial(get_taskgraph_command_parser, "full"),
)
def taskgraph_full(command_context, **options):
    return run_show_taskgraph(command_context, **options)


@SubCommand(
    "taskgraph",
    "target",
    description="Show the target task set",
    parser=partial(get_taskgraph_command_parser, "target"),
)
def taskgraph_target(command_context, **options):
    return run_show_taskgraph(command_context, **options)


@SubCommand(
    "taskgraph",
    "target-graph",
    description="Show the target taskgraph",
    parser=partial(get_taskgraph_command_parser, "target-graph"),
)
def taskgraph_target_graph(command_context, **options):
    return run_show_taskgraph(command_context, **options)


@SubCommand(
    "taskgraph",
    "optimized",
    description="Show the optimized taskgraph",
    parser=partial(get_taskgraph_command_parser, "optimized"),
)
def taskgraph_optimized(command_context, **options):
    return run_show_taskgraph(command_context, **options)


@SubCommand(
    "taskgraph",
    "morphed",
    description="Show the morphed taskgraph",
    parser=partial(get_taskgraph_command_parser, "morphed"),
)
def taskgraph_morphed(command_context, **options):
    return run_show_taskgraph(command_context, **options)


def run_show_taskgraph(command_context, **options):
    # There are cases where we don't want to set up mach logging (e.g logs
    # are being redirected to disk). By monkeypatching the 'setup_logging'
    # function we can let 'taskgraph.main' decide whether or not to log to
    # the terminal.
    gecko_taskgraph.main.setup_logging = partial(
        setup_logging,
        command_context,
        quiet=options["quiet"],
        verbose=options["verbose"],
    )
    show_taskgraph = options.pop("func")
    return show_taskgraph(options)


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
    help="parameters file (.yml or .json; see `taskcluster/docs/parameters.rst`)`",
)
def taskgraph_actions(command_context, **options):
    return show_actions(command_context, options)


@SubCommand(
    "taskgraph",
    "decision",
    description="Run the decision task",
    parser=get_taskgraph_decision_parser,
)
def taskgraph_decision(command_context, **options):
    """Run the decision task: generate a task graph and submit to
    TaskCluster.  This is only meant to be called within decision tasks,
    and requires a great many arguments.  Commands like `mach taskgraph
    optimized` are better suited to use on the command line, and can take
    the parameters file generated by a decision task."""
    try:
        setup_logging(command_context)
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
def taskgraph_cron(command_context, **options):
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
def action_callback(command_context, **options):
    setup_logging(command_context)
    taskgraph_commands["action-callback"].func(options)


@SubCommand(
    "taskgraph",
    "test-action-callback",
    description="Run an action callback in a testing mode",
    parser=partial(get_taskgraph_command_parser, "test-action-callback"),
)
def test_action_callback(command_context, **options):
    setup_logging(command_context)

    if not options["parameters"]:
        options["parameters"] = "project=mozilla-central"

    taskgraph_commands["test-action-callback"].func(options)


def setup_logging(command_context, quiet=False, verbose=True):
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


def show_actions(command_context, options):
    import gecko_taskgraph
    import gecko_taskgraph.actions
    from taskgraph.generator import TaskGraphGenerator
    from taskgraph.parameters import parameters_loader

    try:
        setup_logging(
            command_context, quiet=options["quiet"], verbose=options["verbose"]
        )
        parameters = parameters_loader(options["parameters"])

        tgg = TaskGraphGenerator(
            root_dir=options.get("root"),
            parameters=parameters,
        )

        actions = gecko_taskgraph.actions.render_actions_json(
            tgg.parameters,
            tgg.graph_config,
            decision_task_id="DECISION-TASK",
        )
        print(json.dumps(actions, sort_keys=True, indent=2, separators=(",", ": ")))
    except Exception:
        traceback.print_exc()
        sys.exit(1)


@Command(
    "taskcluster-load-image",
    category="ci",
    description="Load a pre-built Docker image. Note that you need to "
    "have docker installed and running for this to work.",
    parser=partial(get_taskgraph_command_parser, "load-image"),
)
def load_image(command_context, **kwargs):
    taskgraph_commands["load-image"].func(kwargs)


@Command(
    "taskcluster-build-image",
    category="ci",
    description="Build a Docker image",
    parser=partial(get_taskgraph_command_parser, "build-image"),
)
def build_image(command_context, **kwargs):
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
def image_digest(command_context, **kwargs):
    taskgraph_commands["image-digest"].func(kwargs)


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
def generate_partials_builds(command_context, product, branch):
    from gecko_taskgraph.util.partials import populate_release_history

    try:
        import yaml

        release_history = {"release_history": populate_release_history(product, branch)}
        print(
            yaml.safe_dump(
                release_history, allow_unicode=True, default_flow_style=False
            )
        )
    except Exception:
        traceback.print_exc()
        sys.exit(1)
