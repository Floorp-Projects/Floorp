# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

import json
import logging
import os
import sys
import traceback
import re

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import MachCommandBase


class ShowTaskGraphSubCommand(SubCommand):
    """A SubCommand with TaskGraph-specific arguments"""

    def __call__(self, func):
        after = SubCommand.__call__(self, func)
        args = [
            CommandArgument('--root', '-r',
                            help="root of the taskgraph definition relative to topsrcdir"),
            CommandArgument('--quiet', '-q', action="store_true",
                            help="suppress all logging output"),
            CommandArgument('--verbose', '-v', action="store_true",
                            help="include debug-level logging output"),
            CommandArgument('--json', '-J', action="store_const",
                            dest="format", const="json",
                            help="Output task graph as a JSON object"),
            CommandArgument('--labels', '-L', action="store_const",
                            dest="format", const="labels",
                            help="Output the label for each task in the task graph (default)"),
            CommandArgument('--parameters', '-p', default="project=mozilla-central",
                            help="parameters file (.yml or .json; see "
                                 "`taskcluster/docs/parameters.rst`)`"),
            CommandArgument('--no-optimize', dest="optimize", action="store_false",
                            default="true",
                            help="do not remove tasks from the graph that are found in the "
                            "index (a.k.a. optimize the graph)"),
            CommandArgument('--tasks-regex', '--tasks', default=None,
                            help="only return tasks with labels matching this regular "
                            "expression."),
            CommandArgument('-F', '--fast', dest='fast', default=False, action='store_true',
                            help="enable fast task generation for local debugging.")

        ]
        for arg in args:
            after = arg(after)
        return after


@CommandProvider
class MachCommands(MachCommandBase):

    @Command('taskgraph', category="ci",
             description="Manipulate TaskCluster task graphs defined in-tree")
    def taskgraph(self):
        """The taskgraph subcommands all relate to the generation of task graphs
        for Gecko continuous integration.  A task graph is a set of tasks linked
        by dependencies: for example, a binary must be built before it is tested,
        and that build may further depend on various toolchains, libraries, etc.
        """

    @ShowTaskGraphSubCommand('taskgraph', 'tasks',
                             description="Show all tasks in the taskgraph")
    def taskgraph_tasks(self, **options):
        return self.show_taskgraph('full_task_set', options)

    @ShowTaskGraphSubCommand('taskgraph', 'full',
                             description="Show the full taskgraph")
    def taskgraph_full(self, **options):
        return self.show_taskgraph('full_task_graph', options)

    @ShowTaskGraphSubCommand('taskgraph', 'target',
                             description="Show the target task set")
    def taskgraph_target(self, **options):
        return self.show_taskgraph('target_task_set', options)

    @ShowTaskGraphSubCommand('taskgraph', 'target-graph',
                             description="Show the target taskgraph")
    def taskgraph_target_taskgraph(self, **options):
        return self.show_taskgraph('target_task_graph', options)

    @ShowTaskGraphSubCommand('taskgraph', 'optimized',
                             description="Show the optimized taskgraph")
    def taskgraph_optimized(self, **options):
        return self.show_taskgraph('optimized_task_graph', options)

    @ShowTaskGraphSubCommand('taskgraph', 'morphed',
                             description="Show the morphed taskgraph")
    def taskgraph_morphed(self, **options):
        return self.show_taskgraph('morphed_task_graph', options)

    @SubCommand('taskgraph', 'actions',
                description="Write actions.json to stdout")
    @CommandArgument('--root', '-r',
                     help="root of the taskgraph definition relative to topsrcdir")
    @CommandArgument('--quiet', '-q', action="store_true",
                     help="suppress all logging output")
    @CommandArgument('--verbose', '-v', action="store_true",
                     help="include debug-level logging output")
    @CommandArgument('--parameters', '-p', default="project=mozilla-central",
                     help="parameters file (.yml or .json; see "
                          "`taskcluster/docs/parameters.rst`)`")
    def taskgraph_actions(self, **options):
        return self.show_actions(options)

    @SubCommand('taskgraph', 'decision',
                description="Run the decision task")
    @CommandArgument('--root', '-r',
                     help="root of the taskgraph definition relative to topsrcdir")
    @CommandArgument('--base-repository',
                     required=True,
                     help='URL for "base" repository to clone')
    @CommandArgument('--head-repository',
                     required=True,
                     help='URL for "head" repository to fetch revision from')
    @CommandArgument('--head-ref',
                     required=True,
                     help='Reference (this is same as rev usually for hg)')
    @CommandArgument('--head-rev',
                     required=True,
                     help='Commit revision to use from head repository')
    @CommandArgument('--comm-base-repository',
                     required=False,
                     help='URL for "base" comm-* repository to clone')
    @CommandArgument('--comm-head-repository',
                     required=False,
                     help='URL for "head" comm-* repository to fetch revision from')
    @CommandArgument('--comm-head-ref',
                     required=False,
                     help='comm-* Reference (this is same as rev usually for hg)')
    @CommandArgument('--comm-head-rev',
                     required=False,
                     help='Commit revision to use from head comm-* repository')
    @CommandArgument('--message',
                     required=True,
                     help='Commit message to be parsed. Example: "try: -b do -p all -u all"')
    @CommandArgument('--project',
                     required=True,
                     help='Project to use for creating task graph. Example: --project=try')
    @CommandArgument('--pushlog-id',
                     dest='pushlog_id',
                     required=True,
                     default=0)
    @CommandArgument('--pushdate',
                     dest='pushdate',
                     required=True,
                     type=int,
                     default=0)
    @CommandArgument('--owner',
                     required=True,
                     help='email address of who owns this graph')
    @CommandArgument('--level',
                     required=True,
                     help='SCM level of this repository')
    @CommandArgument('--target-tasks-method',
                     help='method for selecting the target tasks to generate')
    @CommandArgument('--try-task-config-file',
                     help='path to try task configuration file')
    def taskgraph_decision(self, **options):
        """Run the decision task: generate a task graph and submit to
        TaskCluster.  This is only meant to be called within decision tasks,
        and requires a great many arguments.  Commands like `mach taskgraph
        optimized` are better suited to use on the command line, and can take
        the parameters file generated by a decision task.  """

        import taskgraph.decision
        try:
            self.setup_logging()
            return taskgraph.decision.taskgraph_decision(options)
        except Exception:
            traceback.print_exc()
            sys.exit(1)

    @SubCommand('taskgraph', 'cron',
                description="Run the cron task")
    @CommandArgument('--base-repository',
                     required=False,
                     help='(ignored)')
    @CommandArgument('--head-repository',
                     required=True,
                     help='URL for "head" repository to fetch')
    @CommandArgument('--head-ref',
                     required=False,
                     help='(ignored)')
    @CommandArgument('--project',
                     required=True,
                     help='Project to use for creating tasks. Example: --project=mozilla-central')
    @CommandArgument('--level',
                     required=True,
                     help='SCM level of this repository')
    @CommandArgument('--force-run',
                     required=False,
                     help='If given, force this cronjob to run regardless of time, '
                     'and run no others')
    @CommandArgument('--no-create',
                     required=False,
                     action='store_true',
                     help='Do not actually create tasks')
    @CommandArgument('--root', '-r',
                     required=False,
                     help="root of the repository to get cron task definitions from")
    def taskgraph_cron(self, **options):
        """Run the cron task; this task creates zero or more decision tasks.  It is run
        from the hooks service on a regular basis."""
        import taskgraph.cron
        try:
            self.setup_logging()
            return taskgraph.cron.taskgraph_cron(options)
        except Exception:
            traceback.print_exc()
            sys.exit(1)

    @SubCommand('taskgraph', 'action-callback',
                description='Run action callback used by action tasks')
    @CommandArgument('--root', '-r', default='taskcluster/ci',
                     help="root of the taskgraph definition relative to topsrcdir")
    def action_callback(self, **options):
        import taskgraph.actions
        try:
            self.setup_logging()

            task_group_id = os.environ.get('ACTION_TASK_GROUP_ID', None)
            task_id = json.loads(os.environ.get('ACTION_TASK_ID', 'null'))
            input = json.loads(os.environ.get('ACTION_INPUT', 'null'))
            callback = os.environ.get('ACTION_CALLBACK', None)
            parameters = json.loads(os.environ.get('ACTION_PARAMETERS', '{}'))
            root = options['root']

            return taskgraph.actions.trigger_action_callback(
                    task_group_id=task_group_id,
                    task_id=task_id,
                    input=input,
                    callback=callback,
                    parameters=parameters,
                    root=root,
                    test=False)
        except Exception:
            traceback.print_exc()
            sys.exit(1)

    @SubCommand('taskgraph', 'test-action-callback',
                description='Run an action callback in a testing mode')
    @CommandArgument('--root', '-r', default='taskcluster/ci',
                     help="root of the taskgraph definition relative to topsrcdir")
    @CommandArgument('--parameters', '-p', default='project=mozilla-central',
                     help='parameters file (.yml or .json; see '
                          '`taskcluster/docs/parameters.rst`)`')
    @CommandArgument('--task-id', default=None,
                     help='TaskId to which the action applies')
    @CommandArgument('--task-group-id', default=None,
                     help='TaskGroupId to which the action applies')
    @CommandArgument('--input', default=None,
                     help='Action input (.yml or .json)')
    @CommandArgument('--task', default=None,
                     help='Task definition (.yml or .json; if omitted, the task will be'
                          'fetched from the queue)')
    @CommandArgument('callback', default=None,
                     help='Action callback name (Python function name)')
    def test_action_callback(self, **options):
        import taskgraph.parameters
        import taskgraph.actions
        import yaml

        def load_data(filename):
            with open(filename) as f:
                if filename.endswith('.yml'):
                    return yaml.safe_load(f)
                elif filename.endswith('.json'):
                    return json.load(f)
                else:
                    raise Exception("unknown filename {}".format(filename))

        try:
            self.setup_logging()
            task_id = options['task_id']

            if options['input']:
                input = load_data(options['input'])
            else:
                input = None

            parameters = taskgraph.parameters.load_parameters_file(options['parameters'])
            parameters.check()

            root = options['root']

            return taskgraph.actions.trigger_action_callback(
                    task_group_id=options['task_group_id'],
                    task_id=task_id,
                    input=input,
                    callback=options['callback'],
                    parameters=parameters,
                    root=root,
                    test=True)
        except Exception:
            traceback.print_exc()
            sys.exit(1)

    def setup_logging(self, quiet=False, verbose=True):
        """
        Set up Python logging for all loggers, sending results to stderr (so
        that command output can be redirected easily) and adding the typical
        mach timestamp.
        """
        # remove the old terminal handler
        old = self.log_manager.replace_terminal_handler(None)

        # re-add it, with level and fh set appropriately
        if not quiet:
            level = logging.DEBUG if verbose else logging.INFO
            self.log_manager.add_terminal_logging(
                fh=sys.stderr, level=level,
                write_interval=old.formatter.write_interval,
                write_times=old.formatter.write_times)

        # all of the taskgraph logging is unstructured logging
        self.log_manager.enable_unstructured()

    def show_taskgraph(self, graph_attr, options):
        import taskgraph.parameters
        import taskgraph.target_tasks
        import taskgraph.generator
        import taskgraph
        if options['fast']:
            taskgraph.fast = True

        try:
            self.setup_logging(quiet=options['quiet'], verbose=options['verbose'])
            parameters = taskgraph.parameters.load_parameters_file(options['parameters'])
            parameters.check()

            tgg = taskgraph.generator.TaskGraphGenerator(
                root_dir=options.get('root'),
                parameters=parameters)

            tg = getattr(tgg, graph_attr)

            show_method = getattr(self, 'show_taskgraph_' + (options['format'] or 'labels'))
            tg = self.get_filtered_taskgraph(tg, options["tasks_regex"])
            show_method(tg)
        except Exception:
            traceback.print_exc()
            sys.exit(1)

    def show_taskgraph_labels(self, taskgraph):
        for index in taskgraph.graph.visit_postorder():
            print(taskgraph.tasks[index].label)

    def show_taskgraph_json(self, taskgraph):
        print(json.dumps(taskgraph.to_json(),
              sort_keys=True, indent=2, separators=(',', ': ')))

    def get_filtered_taskgraph(self, taskgraph, tasksregex):
        from taskgraph.graph import Graph
        from taskgraph.taskgraph import TaskGraph
        """
        This class method filters all the tasks on basis of a regular expression
        and returns a new TaskGraph object
        """
        # return original taskgraph if no regular expression is passed
        if not tasksregex:
            return taskgraph
        named_links_dict = taskgraph.graph.named_links_dict()
        filteredtasks = {}
        filterededges = set()
        regexprogram = re.compile(tasksregex)

        for key in taskgraph.graph.visit_postorder():
            task = taskgraph.tasks[key]
            if regexprogram.match(task.label):
                filteredtasks[key] = task
                for depname, dep in named_links_dict[key].iteritems():
                    if regexprogram.match(dep):
                        filterededges.add((key, dep, depname))
        filtered_taskgraph = TaskGraph(filteredtasks, Graph(set(filteredtasks), filterededges))
        return filtered_taskgraph

    def show_actions(self, options):
        import taskgraph.parameters
        import taskgraph.target_tasks
        import taskgraph.generator
        import taskgraph
        import taskgraph.actions

        try:
            self.setup_logging(quiet=options['quiet'], verbose=options['verbose'])
            parameters = taskgraph.parameters.load_parameters_file(options['parameters'])
            parameters.check()

            tgg = taskgraph.generator.TaskGraphGenerator(
                root_dir=options.get('root'),
                parameters=parameters)

            actions = taskgraph.actions.render_actions_json(parameters, tgg.graph_config)
            print(json.dumps(actions, sort_keys=True, indent=2, separators=(',', ': ')))
        except Exception:
            traceback.print_exc()
            sys.exit(1)


@CommandProvider
class TaskClusterImagesProvider(MachCommandBase):
    def _ensure_zstd(self):
        try:
            import zstandard  # noqa: F401
        except (ImportError, AttributeError):
            self._activate_virtualenv()
            self.virtualenv_manager.install_pip_package('zstandard==0.9.0')

    @Command('taskcluster-load-image', category="ci",
             description="Load a pre-built Docker image")
    @CommandArgument('--task-id',
                     help="Load the image at public/image.tar.zst in this task,"
                          "rather than searching the index")
    @CommandArgument('-t', '--tag',
                     help="tag that the image should be loaded as. If not "
                          "image will be loaded with tag from the tarball",
                     metavar="name:tag")
    @CommandArgument('image_name', nargs='?',
                     help="Load the image of this name based on the current"
                          "contents of the tree (as built for mozilla-central"
                          "or mozilla-inbound)")
    def load_image(self, image_name, task_id, tag):
        self._ensure_zstd()
        from taskgraph.docker import load_image_by_name, load_image_by_task_id
        if not image_name and not task_id:
            print("Specify either IMAGE-NAME or TASK-ID")
            sys.exit(1)
        try:
            if task_id:
                ok = load_image_by_task_id(task_id, tag)
            else:
                ok = load_image_by_name(image_name, tag)
            if not ok:
                sys.exit(1)
        except Exception:
            traceback.print_exc()
            sys.exit(1)

    @Command('taskcluster-build-image', category='ci',
             description='Build a Docker image')
    @CommandArgument('image_name',
                     help='Name of the image to build')
    @CommandArgument('-t', '--tag',
                     help="tag that the image should be built as.",
                     metavar="name:tag")
    @CommandArgument('--context-only',
                     help="File name the context tarball should be written to."
                          "with this option it will only build the context.tar.",
                     metavar='context.tar')
    def build_image(self, image_name, tag, context_only):
        from taskgraph.docker import build_image, build_context
        try:
            if context_only is None:
                build_image(image_name, tag, os.environ)
            else:
                build_context(image_name, context_only, os.environ)
        except Exception:
            traceback.print_exc()
            sys.exit(1)


@CommandProvider
class TaskClusterPartialsData(object):
    @Command('release-history', category="ci",
             description="Query balrog for release history used by enable partials generation")
    @CommandArgument('-b', '--branch',
                     help="The gecko project branch used in balrog, such as "
                          "mozilla-central, release, maple")
    @CommandArgument('--product', default='Firefox',
                     help="The product identifier, such as 'Firefox'")
    def generate_partials_builds(self, product, branch):
        from taskgraph.util.partials import populate_release_history
        try:
            import yaml
            release_history = {'release_history': populate_release_history(product, branch)}
            print(yaml.safe_dump(release_history, allow_unicode=True, default_flow_style=False))
        except Exception:
            traceback.print_exc()
            sys.exit(1)
