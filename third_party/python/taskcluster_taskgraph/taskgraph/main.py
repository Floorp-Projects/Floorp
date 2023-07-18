# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import atexit
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
from collections import namedtuple
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from typing import Any, List

import appdirs
import yaml

Command = namedtuple("Command", ["func", "args", "kwargs", "defaults"])
commands = {}


def command(*args, **kwargs):
    defaults = kwargs.pop("defaults", {})

    def decorator(func):
        commands[args[0]] = Command(func, args, kwargs, defaults)
        return func

    return decorator


def argument(*args, **kwargs):
    def decorator(func):
        if not hasattr(func, "args"):
            func.args = []
        func.args.append((args, kwargs))
        return func

    return decorator


def format_taskgraph_labels(taskgraph):
    return "\n".join(
        sorted(
            taskgraph.tasks[index].label for index in taskgraph.graph.visit_postorder()
        )
    )


def format_taskgraph_json(taskgraph):
    return json.dumps(
        taskgraph.to_json(), sort_keys=True, indent=2, separators=(",", ": ")
    )


def format_taskgraph_yaml(taskgraph):
    return yaml.safe_dump(taskgraph.to_json(), default_flow_style=False)


def get_filtered_taskgraph(taskgraph, tasksregex, exclude_keys):
    """
    Filter all the tasks on basis of a regular expression
    and returns a new TaskGraph object
    """
    from taskgraph.graph import Graph
    from taskgraph.task import Task
    from taskgraph.taskgraph import TaskGraph

    if tasksregex:
        named_links_dict = taskgraph.graph.named_links_dict()
        filteredtasks = {}
        filterededges = set()
        regexprogram = re.compile(tasksregex)

        for key in taskgraph.graph.visit_postorder():
            task = taskgraph.tasks[key]
            if regexprogram.match(task.label):
                filteredtasks[key] = task
                for depname, dep in named_links_dict[key].items():
                    if regexprogram.match(dep):
                        filterededges.add((key, dep, depname))

        taskgraph = TaskGraph(filteredtasks, Graph(set(filteredtasks), filterededges))

    if exclude_keys:
        for label, task in taskgraph.tasks.items():
            task = task.to_json()
            for key in exclude_keys:
                obj = task
                attrs = key.split(".")
                while attrs[0] in obj:
                    if len(attrs) == 1:
                        del obj[attrs[0]]
                        break
                    obj = obj[attrs[0]]
                    attrs = attrs[1:]
            taskgraph.tasks[label] = Task.from_json(task)

    return taskgraph


FORMAT_METHODS = {
    "labels": format_taskgraph_labels,
    "json": format_taskgraph_json,
    "yaml": format_taskgraph_yaml,
}


def get_taskgraph_generator(root, parameters):
    """Helper function to make testing a little easier."""
    from taskgraph.generator import TaskGraphGenerator

    return TaskGraphGenerator(root_dir=root, parameters=parameters)


def format_taskgraph(options, parameters, logfile=None):
    import taskgraph
    from taskgraph.parameters import parameters_loader

    if logfile:
        handler = logging.FileHandler(logfile, mode="w")
        if logging.root.handlers:
            oldhandler = logging.root.handlers[-1]
            logging.root.removeHandler(oldhandler)
            handler.setFormatter(oldhandler.formatter)
        logging.root.addHandler(handler)

    if options["fast"]:
        taskgraph.fast = True

    if isinstance(parameters, str):
        parameters = parameters_loader(
            parameters,
            overrides={"target-kind": options.get("target_kind")},
            strict=False,
        )

    tgg = get_taskgraph_generator(options.get("root"), parameters)

    tg = getattr(tgg, options["graph_attr"])
    tg = get_filtered_taskgraph(tg, options["tasks_regex"], options["exclude_keys"])
    format_method = FORMAT_METHODS[options["format"] or "labels"]
    return format_method(tg)


def dump_output(out, path=None, params_spec=None):
    from taskgraph.parameters import Parameters

    params_name = Parameters.format_spec(params_spec)
    fh = None
    if path:
        # Substitute params name into file path if necessary
        if params_spec and "{params}" not in path:
            name, ext = os.path.splitext(path)
            name += "_{params}"
            path = name + ext

        path = path.format(params=params_name)
        fh = open(path, "w")
    else:
        print(
            f"Dumping result with parameters from {params_name}:",
            file=sys.stderr,
        )
    print(out + "\n", file=fh)


def generate_taskgraph(options, parameters, logdir):
    from taskgraph.parameters import Parameters

    def logfile(spec):
        """Determine logfile given a parameters specification."""
        if logdir is None:
            return None
        return os.path.join(
            logdir,
            "{}_{}.log".format(options["graph_attr"], Parameters.format_spec(spec)),
        )

    # Don't bother using futures if there's only one parameter. This can make
    # tracebacks a little more readable and avoids additional process overhead.
    if len(parameters) == 1:
        spec = parameters[0]
        out = format_taskgraph(options, spec, logfile(spec))
        dump_output(out, options["output_file"])
        return

    futures = {}
    with ProcessPoolExecutor() as executor:
        for spec in parameters:
            f = executor.submit(format_taskgraph, options, spec, logfile(spec))
            futures[f] = spec

    for future in as_completed(futures):
        output_file = options["output_file"]
        spec = futures[future]
        e = future.exception()
        if e:
            out = "".join(traceback.format_exception(type(e), e, e.__traceback__))
            if options["diff"]:
                # Dump to console so we don't accidentally diff the tracebacks.
                output_file = None
        else:
            out = future.result()

        dump_output(
            out,
            path=output_file,
            params_spec=spec if len(parameters) > 1 else None,
        )


@command(
    "tasks",
    help="Show all tasks in the taskgraph.",
    defaults={"graph_attr": "full_task_set"},
)
@command(
    "full", help="Show the full taskgraph.", defaults={"graph_attr": "full_task_graph"}
)
@command(
    "target",
    help="Show the set of target tasks.",
    defaults={"graph_attr": "target_task_set"},
)
@command(
    "target-graph",
    help="Show the target graph.",
    defaults={"graph_attr": "target_task_graph"},
)
@command(
    "optimized",
    help="Show the optimized graph.",
    defaults={"graph_attr": "optimized_task_graph"},
)
@command(
    "morphed",
    help="Show the morphed graph.",
    defaults={"graph_attr": "morphed_task_graph"},
)
@argument("--root", "-r", help="root of the taskgraph definition relative to topsrcdir")
@argument("--quiet", "-q", action="store_true", help="suppress all logging output")
@argument(
    "--verbose", "-v", action="store_true", help="include debug-level logging output"
)
@argument(
    "--json",
    "-J",
    action="store_const",
    dest="format",
    const="json",
    help="Output task graph as a JSON object",
)
@argument(
    "--yaml",
    "-Y",
    action="store_const",
    dest="format",
    const="yaml",
    help="Output task graph as a YAML object",
)
@argument(
    "--labels",
    "-L",
    action="store_const",
    dest="format",
    const="labels",
    help="Output the label for each task in the task graph (default)",
)
@argument(
    "--parameters",
    "-p",
    default=None,
    action="append",
    help="Parameters to use for the generation. Can be a path to file (.yml or "
    ".json; see `taskcluster/docs/parameters.rst`), a directory (containing "
    "parameters files), a url, of the form `project=mozilla-central` to download "
    "latest parameters file for the specified project from CI, or of the form "
    "`task-id=<decision task id>` to download parameters from the specified "
    "decision task. Can be specified multiple times, in which case multiple "
    "generations will happen from the same invocation (one per parameters "
    "specified).",
)
@argument(
    "--no-optimize",
    dest="optimize",
    action="store_false",
    default="true",
    help="do not remove tasks from the graph that are found in the "
    "index (a.k.a. optimize the graph)",
)
@argument(
    "-o",
    "--output-file",
    default=None,
    help="file path to store generated output.",
)
@argument(
    "--tasks-regex",
    "--tasks",
    default=None,
    help="only return tasks with labels matching this regular " "expression.",
)
@argument(
    "--exclude-key",
    default=None,
    dest="exclude_keys",
    action="append",
    help="Exclude the specified key (using dot notation) from the final result. "
    "This is mainly useful with '--diff' to filter out expected differences. Can be "
    "used multiple times.",
)
@argument(
    "--target-kind",
    default=None,
    help="only return tasks that are of the given kind, or their dependencies.",
)
@argument(
    "-F",
    "--fast",
    default=False,
    action="store_true",
    help="enable fast task generation for local debugging.",
)
@argument(
    "--diff",
    const="default",
    nargs="?",
    default=None,
    help="Generate and diff the current taskgraph against another revision. "
    "Without args the base revision will be used. A revision specifier such as "
    "the hash or `.~1` (hg) or `HEAD~1` (git) can be used as well.",
)
def show_taskgraph(options):
    from taskgraph.parameters import Parameters, parameters_loader
    from taskgraph.util.vcs import get_repository

    if options.pop("verbose", False):
        logging.root.setLevel(logging.DEBUG)

    repo = None
    cur_rev = None
    diffdir = None
    output_file = options["output_file"]

    if options["diff"]:
        repo = get_repository(os.getcwd())

        if not repo.working_directory_clean():
            print(
                "abort: can't diff taskgraph with dirty working directory",
                file=sys.stderr,
            )
            return 1

        # We want to return the working directory to the current state
        # as best we can after we're done. In all known cases, using
        # branch or bookmark (which are both available on the VCS object)
        # as `branch` is preferable to a specific revision.
        cur_rev = repo.branch or repo.head_rev[:12]

        diffdir = tempfile.mkdtemp()
        atexit.register(
            shutil.rmtree, diffdir
        )  # make sure the directory gets cleaned up
        options["output_file"] = os.path.join(
            diffdir, f"{options['graph_attr']}_{cur_rev}"
        )
        print(f"Generating {options['graph_attr']} @ {cur_rev}", file=sys.stderr)

    parameters: List[Any[str, Parameters]] = options.pop("parameters")
    if not parameters:
        overrides = {
            "target-kind": options.get("target_kind"),
        }
        parameters = [
            parameters_loader(None, strict=False, overrides=overrides)
        ]  # will use default values

    for param in parameters[:]:
        if isinstance(param, str) and os.path.isdir(param):
            parameters.remove(param)
            parameters.extend(
                [
                    p.as_posix()
                    for p in Path(param).iterdir()
                    if p.suffix in (".yml", ".json")
                ]
            )

    logdir = None
    if len(parameters) > 1:
        # Log to separate files for each process instead of stderr to
        # avoid interleaving.
        basename = os.path.basename(os.getcwd())
        logdir = os.path.join(appdirs.user_log_dir("taskgraph"), basename)
        if not os.path.isdir(logdir):
            os.makedirs(logdir)
    else:
        # Only setup logging if we have a single parameter spec. Otherwise
        # logging will go to files. This is also used as a hook for Gecko
        # to setup its `mach` based logging.
        setup_logging()

    generate_taskgraph(options, parameters, logdir)

    if options["diff"]:
        assert diffdir is not None
        assert repo is not None

        # Reload taskgraph modules to pick up changes and clear global state.
        for mod in sys.modules.copy():
            if mod != __name__ and mod.split(".", 1)[0].endswith("taskgraph"):
                del sys.modules[mod]

        if options["diff"] == "default":
            base_rev = repo.base_rev
        else:
            base_rev = options["diff"]

        try:
            repo.update(base_rev)
            base_rev = repo.head_rev[:12]
            options["output_file"] = os.path.join(
                diffdir, f"{options['graph_attr']}_{base_rev}"
            )
            print(f"Generating {options['graph_attr']} @ {base_rev}", file=sys.stderr)
            generate_taskgraph(options, parameters, logdir)
        finally:
            repo.update(cur_rev)

        # Generate diff(s)
        diffcmd = [
            "diff",
            "-U20",
            "--report-identical-files",
            f"--label={options['graph_attr']}@{base_rev}",
            f"--label={options['graph_attr']}@{cur_rev}",
        ]

        for spec in parameters:
            base_path = os.path.join(diffdir, f"{options['graph_attr']}_{base_rev}")
            cur_path = os.path.join(diffdir, f"{options['graph_attr']}_{cur_rev}")

            params_name = None
            if len(parameters) > 1:
                params_name = Parameters.format_spec(spec)
                base_path += f"_{params_name}"
                cur_path += f"_{params_name}"

            try:
                proc = subprocess.run(
                    diffcmd + [base_path, cur_path],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True,
                    check=True,
                )
                diff_output = proc.stdout
                returncode = 0
            except subprocess.CalledProcessError as e:
                # returncode 1 simply means diffs were found
                if e.returncode != 1:
                    print(e.stderr, file=sys.stderr)
                    raise
                diff_output = e.output
                returncode = e.returncode

            dump_output(
                diff_output,
                # Don't bother saving file if no diffs were found. Log to
                # console in this case instead.
                path=None if returncode == 0 else output_file,
                params_spec=spec if len(parameters) > 1 else None,
            )

        if options["format"] != "json":
            print(
                "If you were expecting differences in task bodies "
                'you should pass "-J"\n',
                file=sys.stderr,
            )

    if len(parameters) > 1:
        print(f"See '{logdir}' for logs", file=sys.stderr)


@command("build-image", help="Build a Docker image")
@argument("image_name", help="Name of the image to build")
@argument(
    "-t", "--tag", help="tag that the image should be built as.", metavar="name:tag"
)
@argument(
    "--context-only",
    help="File name the context tarball should be written to."
    "with this option it will only build the context.tar.",
    metavar="context.tar",
)
def build_image(args):
    from taskgraph.docker import build_context, build_image

    if args["context_only"] is None:
        build_image(args["image_name"], args["tag"], os.environ)
    else:
        build_context(args["image_name"], args["context_only"], os.environ)


@command(
    "load-image",
    help="Load a pre-built Docker image. Note that you need to "
    "have docker installed and running for this to work.",
)
@argument(
    "--task-id",
    help="Load the image at public/image.tar.zst in this task, "
    "rather than searching the index",
)
@argument(
    "-t",
    "--tag",
    help="tag that the image should be loaded as. If not "
    "image will be loaded with tag from the tarball",
    metavar="name:tag",
)
@argument(
    "image_name",
    nargs="?",
    help="Load the image of this name based on the current "
    "contents of the tree (as built for mozilla-central "
    "or mozilla-inbound)",
)
def load_image(args):
    from taskgraph.docker import load_image_by_name, load_image_by_task_id

    if not args.get("image_name") and not args.get("task_id"):
        print("Specify either IMAGE-NAME or TASK-ID")
        sys.exit(1)
    try:
        if args["task_id"]:
            ok = load_image_by_task_id(args["task_id"], args.get("tag"))
        else:
            ok = load_image_by_name(args["image_name"], args.get("tag"))
        if not ok:
            sys.exit(1)
    except Exception:
        traceback.print_exc()
        sys.exit(1)


@command("image-digest", help="Print the digest of a docker image.")
@argument(
    "image_name",
    help="Print the digest of the image of this name based on the current "
    "contents of the tree.",
)
def image_digest(args):
    from taskgraph.docker import get_image_digest

    try:
        digest = get_image_digest(args["image_name"])
        print(digest)
    except Exception:
        traceback.print_exc()
        sys.exit(1)


@command("decision", help="Run the decision task")
@argument("--root", "-r", help="root of the taskgraph definition relative to topsrcdir")
@argument(
    "--message",
    required=False,
    help=argparse.SUPPRESS,
)
@argument(
    "--project",
    required=True,
    help="Project to use for creating task graph. Example: --project=try",
)
@argument("--pushlog-id", dest="pushlog_id", required=True, default="0")
@argument("--pushdate", dest="pushdate", required=True, type=int, default=0)
@argument("--owner", required=True, help="email address of who owns this graph")
@argument("--level", required=True, help="SCM level of this repository")
@argument(
    "--target-tasks-method", help="method for selecting the target tasks to generate"
)
@argument(
    "--repository-type",
    required=True,
    help='Type of repository, either "hg" or "git"',
)
@argument("--base-repository", required=True, help='URL for "base" repository to clone')
@argument(
    "--base-ref", default="", help='Reference of the revision in the "base" repository'
)
@argument(
    "--base-rev",
    default="",
    help="Taskgraph decides what to do based on the revision range between "
    "`--base-rev` and `--head-rev`. Value is determined automatically if not provided",
)
@argument(
    "--head-repository",
    required=True,
    help='URL for "head" repository to fetch revision from',
)
@argument(
    "--head-ref", required=True, help="Reference (this is same as rev usually for hg)"
)
@argument(
    "--head-rev", required=True, help="Commit revision to use from head repository"
)
@argument("--head-tag", help="Tag attached to the revision", default="")
@argument(
    "--tasks-for", required=True, help="the tasks_for value used to generate this task"
)
@argument("--try-task-config-file", help="path to try task configuration file")
def decision(options):
    from taskgraph.decision import taskgraph_decision

    taskgraph_decision(options)


@command("action-callback", description="Run action callback used by action tasks")
@argument(
    "--root",
    "-r",
    default="taskcluster/ci",
    help="root of the taskgraph definition relative to topsrcdir",
)
def action_callback(options):
    from taskgraph.actions import trigger_action_callback
    from taskgraph.actions.util import get_parameters

    try:
        # the target task for this action (or null if it's a group action)
        task_id = json.loads(os.environ.get("ACTION_TASK_ID", "null"))
        # the target task group for this action
        task_group_id = os.environ.get("ACTION_TASK_GROUP_ID", None)
        input = json.loads(os.environ.get("ACTION_INPUT", "null"))
        callback = os.environ.get("ACTION_CALLBACK", None)
        root = options["root"]

        parameters = get_parameters(task_group_id)

        return trigger_action_callback(
            task_group_id=task_group_id,
            task_id=task_id,
            input=input,
            callback=callback,
            parameters=parameters,
            root=root,
            test=False,
        )
    except Exception:
        traceback.print_exc()
        sys.exit(1)


@command("test-action-callback", description="Run an action callback in a testing mode")
@argument(
    "--root",
    "-r",
    default="taskcluster/ci",
    help="root of the taskgraph definition relative to topsrcdir",
)
@argument(
    "--parameters",
    "-p",
    default="",
    help="parameters file (.yml or .json; see " "`taskcluster/docs/parameters.rst`)`",
)
@argument("--task-id", default=None, help="TaskId to which the action applies")
@argument(
    "--task-group-id", default=None, help="TaskGroupId to which the action applies"
)
@argument("--input", default=None, help="Action input (.yml or .json)")
@argument("callback", default=None, help="Action callback name (Python function name)")
def test_action_callback(options):
    import taskgraph.actions
    import taskgraph.parameters
    from taskgraph.config import load_graph_config
    from taskgraph.util import yaml

    def load_data(filename):
        with open(filename) as f:
            if filename.endswith(".yml"):
                return yaml.load_stream(f)
            elif filename.endswith(".json"):
                return json.load(f)
            else:
                raise Exception(f"unknown filename {filename}")

    try:
        task_id = options["task_id"]

        if options["input"]:
            input = load_data(options["input"])
        else:
            input = None

        root = options["root"]
        graph_config = load_graph_config(root)
        trust_domain = graph_config["trust-domain"]
        graph_config.register()

        parameters = taskgraph.parameters.load_parameters_file(
            options["parameters"], strict=False, trust_domain=trust_domain
        )
        parameters.check()

        return taskgraph.actions.trigger_action_callback(
            task_group_id=options["task_group_id"],
            task_id=task_id,
            input=input,
            callback=options["callback"],
            parameters=parameters,
            root=root,
            test=True,
        )
    except Exception:
        traceback.print_exc()
        sys.exit(1)


def create_parser():
    parser = argparse.ArgumentParser(description="Interact with taskgraph")
    subparsers = parser.add_subparsers()
    for _, (func, args, kwargs, defaults) in commands.items():
        subparser = subparsers.add_parser(*args, **kwargs)
        for arg in func.args:
            subparser.add_argument(*arg[0], **arg[1])
        subparser.set_defaults(command=func, **defaults)
    return parser


def setup_logging():
    logging.basicConfig(
        format="%(asctime)s - %(levelname)s - %(message)s", level=logging.INFO
    )


def main(args=sys.argv[1:]):
    setup_logging()
    parser = create_parser()
    args = parser.parse_args(args)
    try:
        args.command(vars(args))
    except Exception:
        traceback.print_exc()
        sys.exit(1)
