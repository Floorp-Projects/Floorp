# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals
import logging
import os
import yaml

from .graph import Graph
from .taskgraph import TaskGraph
from .optimize import optimize_task_graph
from .util.python_path import find_object

logger = logging.getLogger(__name__)


class Kind(object):

    def __init__(self, name, path, config):
        self.name = name
        self.path = path
        self.config = config

    def _get_impl_class(self):
        # load the class defined by implementation
        try:
            impl = self.config['implementation']
        except KeyError:
            raise KeyError("{!r} does not define implementation".format(self.path))
        return find_object(impl)

    def load_tasks(self, parameters, loaded_tasks):
        impl_class = self._get_impl_class()
        return impl_class.load_tasks(self.name, self.path, self.config,
                                     parameters, loaded_tasks)


class TaskGraphGenerator(object):
    """
    The central controller for taskgraph.  This handles all phases of graph
    generation.  The task is generated from all of the kinds defined in
    subdirectories of the generator's root directory.

    Access to the results of this generation, as well as intermediate values at
    various phases of generation, is available via properties.  This encourages
    the provision of all generation inputs at instance construction time.
    """

    # Task-graph generation is implemented as a Python generator that yields
    # each "phase" of generation.  This allows some mach subcommands to short-
    # circuit generation of the entire graph by never completing the generator.

    def __init__(self, root_dir, parameters,
                 target_tasks_method):
        """
        @param root_dir: root directory, with subdirectories for each kind
        @param parameters: parameters for this task-graph generation
        @type parameters: dict
        @param target_tasks_method: function to determine the target_task_set;
                see `./target_tasks.py`.
        @type target_tasks_method: function
        """

        self.root_dir = root_dir
        self.parameters = parameters
        self.target_tasks_method = target_tasks_method

        # this can be set up until the time the target task set is generated;
        # it defaults to parameters['target_tasks']
        self._target_tasks = parameters.get('target_tasks')

        # start the generator
        self._run = self._run()
        self._run_results = {}

    @property
    def full_task_set(self):
        """
        The full task set: all tasks defined by any kind (a graph without edges)

        @type: TaskGraph
        """
        return self._run_until('full_task_set')

    @property
    def full_task_graph(self):
        """
        The full task graph: the full task set, with edges representing
        dependencies.

        @type: TaskGraph
        """
        return self._run_until('full_task_graph')

    @property
    def target_task_set(self):
        """
        The set of targetted tasks (a graph without edges)

        @type: TaskGraph
        """
        return self._run_until('target_task_set')

    @property
    def target_task_graph(self):
        """
        The set of targetted tasks and all of their dependencies

        @type: TaskGraph
        """
        return self._run_until('target_task_graph')

    @property
    def optimized_task_graph(self):
        """
        The set of targetted tasks and all of their dependencies; tasks that
        have been optimized out are either omitted or replaced with a Task
        instance containing only a task_id.

        @type: TaskGraph
        """
        return self._run_until('optimized_task_graph')

    @property
    def label_to_taskid(self):
        """
        A dictionary mapping task label to assigned taskId.  This property helps
        in interpreting `optimized_task_graph`.

        @type: dictionary
        """
        return self._run_until('label_to_taskid')

    def _load_kinds(self):
        for path in os.listdir(self.root_dir):
            path = os.path.join(self.root_dir, path)
            if not os.path.isdir(path):
                continue
            kind_name = os.path.basename(path)
            logger.debug("loading kind `{}` from `{}`".format(kind_name, path))

            kind_yml = os.path.join(path, 'kind.yml')
            with open(kind_yml) as f:
                config = yaml.load(f)

            yield Kind(kind_name, path, config)

    def _run(self):
        logger.info("Loading kinds")
        # put the kinds into a graph and sort topologically so that kinds are loaded
        # in post-order
        kinds = {kind.name: kind for kind in self._load_kinds()}
        edges = set()
        for kind in kinds.itervalues():
            for dep in kind.config.get('kind-dependencies', []):
                edges.add((kind.name, dep, 'kind-dependency'))
        kind_graph = Graph(set(kinds), edges)

        logger.info("Generating full task set")
        all_tasks = {}
        for kind_name in kind_graph.visit_postorder():
            logger.debug("Loading tasks for kind {}".format(kind_name))
            kind = kinds[kind_name]
            new_tasks = kind.load_tasks(self.parameters, list(all_tasks.values()))
            for task in new_tasks:
                if task.label in all_tasks:
                    raise Exception("duplicate tasks with label " + task.label)
                all_tasks[task.label] = task
            logger.info("Generated {} tasks for kind {}".format(len(new_tasks), kind_name))
        full_task_set = TaskGraph(all_tasks, Graph(set(all_tasks), set()))
        yield 'full_task_set', full_task_set

        logger.info("Generating full task graph")
        edges = set()
        for t in full_task_set:
            for dep, depname in t.get_dependencies(full_task_set):
                edges.add((t.label, dep, depname))

        full_task_graph = TaskGraph(all_tasks,
                                    Graph(full_task_set.graph.nodes, edges))
        yield 'full_task_graph', full_task_graph

        logger.info("Generating target task set")
        target_tasks = set(self.target_tasks_method(full_task_graph, self.parameters))
        target_task_set = TaskGraph(
            {l: all_tasks[l] for l in target_tasks},
            Graph(target_tasks, set()))
        yield 'target_task_set', target_task_set

        logger.info("Generating target task graph")
        target_graph = full_task_graph.graph.transitive_closure(target_tasks)
        target_task_graph = TaskGraph(
            {l: all_tasks[l] for l in target_graph.nodes},
            target_graph)
        yield 'target_task_graph', target_task_graph

        logger.info("Generating optimized task graph")
        do_not_optimize = set()
        if not self.parameters.get('optimize_target_tasks', True):
            do_not_optimize = target_task_set.graph.nodes
        optimized_task_graph, label_to_taskid = optimize_task_graph(target_task_graph,
                                                                    do_not_optimize)
        yield 'label_to_taskid', label_to_taskid
        yield 'optimized_task_graph', optimized_task_graph

    def _run_until(self, name):
        while name not in self._run_results:
            try:
                k, v = self._run.next()
            except StopIteration:
                raise AttributeError("No such run result {}".format(name))
            self._run_results[k] = v
        return self._run_results[name]
