# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import sys
import traceback

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
            CommandArgument('--root', '-r', default='taskcluster/ci',
                            help="root of the taskgraph definition relative to topsrcdir"),
            CommandArgument('--quiet', '-q', action="store_true",
                            help="suppress all logging output"),
            CommandArgument('--verbose', '-v', action="store_true",
                            help="include debug-level logging output"),
            CommandArgument('--parameters', '-p', required=True,
                            help="parameters file (.yml or .json; see "
                                 "`taskcluster/docs/parameters.rst`)`"),
            CommandArgument('--no-optimize', dest="optimize", action="store_false",
                            default="true",
                            help="do not remove tasks from the graph that are found in the "
                            "index (a.k.a. optimize the graph)"),
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

    @SubCommand('taskgraph', 'python-tests',
                description='Run the taskgraph unit tests')
    def taskgraph_python_tests(self, **options):
        import unittest
        import mozunit
        suite = unittest.defaultTestLoader.discover('taskgraph.test')
        runner = mozunit.MozTestRunner(verbosity=2)
        result = runner.run(suite)
        if not result.wasSuccessful:
            sys.exit(1)

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

    @SubCommand('taskgraph', 'decision',
                description="Run the decision task")
    @CommandArgument('--root', '-r',
        default='taskcluster/ci',
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
    @CommandArgument('--message',
        required=True,
        help='Commit message to be parsed. Example: "try: -b do -p all -u all"')
    @CommandArgument('--revision-hash',
        required=True,
        help='Treeherder revision hash (long revision id) to attach results to')
    @CommandArgument('--project',
        required=True,
        help='Project to use for creating task graph. Example: --project=try')
    @CommandArgument('--pushlog-id',
        dest='pushlog_id',
        required=True,
        default=0)
    @CommandArgument('--owner',
        required=True,
        help='email address of who owns this graph')
    @CommandArgument('--level',
        required=True,
        help='SCM level of this repository')
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
        except Exception as e:
            traceback.print_exc()
            sys.exit(1)


    def setup_logging(self, quiet=False, verbose=True):
        """
        Set up Python logging for all loggers, sending results to stderr (so
        that command output can be redirected easily) and adding the typical
        mach timestamp.
        """
        # remove the old terminal handler
        self.log_manager.replace_terminal_handler(None)

        # re-add it, with level and fh set appropriately
        if not quiet:
            level = logging.DEBUG if verbose else logging.INFO
            self.log_manager.add_terminal_logging(fh=sys.stderr, level=level)

        # all of the taskgraph logging is unstructured logging
        self.log_manager.enable_unstructured()
        

    def show_taskgraph(self, graph_attr, options):
        import taskgraph.parameters
        import taskgraph.target_tasks
        import taskgraph.generator

        try:
            self.setup_logging(quiet=options['quiet'], verbose=options['verbose'])
            parameters = taskgraph.parameters.load_parameters_file(options)

            target_tasks_method = parameters.get('target_tasks_method', 'all_tasks')
            target_tasks_method = taskgraph.target_tasks.get_method(target_tasks_method)
            tgg = taskgraph.generator.TaskGraphGenerator(
                root_dir=options['root'],
                parameters=parameters,
                target_tasks_method=target_tasks_method)

            tg = getattr(tgg, graph_attr)

            for label in tg.graph.visit_postorder():
                print(tg.tasks[label])
        except Exception as e:
            traceback.print_exc()
            sys.exit(1)
