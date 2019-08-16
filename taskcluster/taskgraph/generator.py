# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals
import logging
import os
import copy
import attr

from . import filter_tasks
from .graph import Graph
from .taskgraph import TaskGraph
from .task import Task
from .optimize import optimize_task_graph
from .morph import morph
from .util.python_path import find_object
from .transforms.base import TransformSequence, TransformConfig
from .util.verify import (
    verify_docs,
    verifications,
)
from .util.yaml import load_yaml
from .config import load_graph_config, GraphConfig

logger = logging.getLogger(__name__)


class KindNotFound(Exception):
    """
    Raised when trying to load kind from a directory without a kind.yml.
    """


@attr.s(frozen=True)
class Kind(object):

    name = attr.ib(type=basestring)
    path = attr.ib(type=basestring)
    config = attr.ib(type=dict)
    graph_config = attr.ib(type=GraphConfig)

    def _get_loader(self):
        try:
            loader = self.config['loader']
        except KeyError:
            raise KeyError("{!r} does not define `loader`".format(self.path))
        return find_object(loader)

    def load_tasks(self, parameters, loaded_tasks):
        loader = self._get_loader()
        config = copy.deepcopy(self.config)

        kind_dependencies = config.get('kind-dependencies', [])
        kind_dependencies_tasks = [task for task in loaded_tasks
                                   if task.kind in kind_dependencies]

        inputs = loader(self.name, self.path, config, parameters, loaded_tasks)

        transforms = TransformSequence()
        for xform_path in config['transforms']:
            transform = find_object(xform_path)
            transforms.add(transform)

        # perform the transformations on the loaded inputs
        trans_config = TransformConfig(self.name, self.path, config, parameters,
                                       kind_dependencies_tasks, self.graph_config)
        tasks = [Task(self.name,
                      label=task_dict['label'],
                      attributes=task_dict['attributes'],
                      task=task_dict['task'],
                      optimization=task_dict.get('optimization'),
                      dependencies=task_dict.get('dependencies'),
                      soft_dependencies=task_dict.get('soft-dependencies'),
                      release_artifacts=task_dict.get('release-artifacts'),
                      )
                 for task_dict in transforms(trans_config, inputs)]
        return tasks

    @classmethod
    def load(cls, root_dir, graph_config, kind_name):
        path = os.path.join(root_dir, kind_name)
        kind_yml = os.path.join(path, 'kind.yml')
        if not os.path.exists(kind_yml):
            raise KindNotFound(kind_yml)

        logger.debug("loading kind `{}` from `{}`".format(kind_name, path))
        config = load_yaml(kind_yml)

        return cls(kind_name, path, config, graph_config)


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

    def __init__(self, root_dir, parameters, target_kind=None):
        """
        @param root_dir: root directory, with subdirectories for each kind
        @param paramaters: parameters for this task-graph generation, or callable
            taking a `GraphConfig` and returning parameters
        @type parameters: Union[Parameters, Callable[[GraphConfig], Parameters]]
        """
        if root_dir is None:
            root_dir = 'taskcluster/ci'
        self.root_dir = root_dir
        self._parameters = parameters
        self._target_kind = target_kind

        # start the generator
        self._run = self._run()
        self._run_results = {}

    @property
    def parameters(self):
        """
        The properties used for this graph.

        @type: Properties
        """
        return self._run_until('parameters')

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

    @property
    def morphed_task_graph(self):
        """
        The optimized task graph, with any subsequent morphs applied. This graph
        will have the same meaning as the optimized task graph, but be in a form
        more palatable to TaskCluster.

        @type: TaskGraph
        """
        return self._run_until('morphed_task_graph')

    @property
    def graph_config(self):
        """
        The configuration for this graph.

        @type: TaskGraph
        """
        return self._run_until('graph_config')

    def _load_kinds(self, graph_config):
        for kind_name in os.listdir(self.root_dir):
            try:
                yield Kind.load(self.root_dir, graph_config, kind_name)
            except KindNotFound:
                continue

    def _run(self):
        logger.info("Loading graph configuration.")
        graph_config = load_graph_config(self.root_dir)

        yield ('graph_config', graph_config)

        if callable(self._parameters):
            parameters = self._parameters(graph_config)
        else:
            parameters = self._parameters
        self.verify_parameters(parameters)

        filters = parameters.get('filters', [])
        # Always add legacy target tasks method until we deprecate that API.
        if 'target_tasks_method' not in filters:
            filters.insert(0, 'target_tasks_method')
        filters = [filter_tasks.filter_task_functions[f] for f in filters]

        yield ('parameters', parameters)

        logger.info("Loading kinds")
        # put the kinds into a graph and sort topologically so that kinds are loaded
        # in post-order
        kinds = {kind.name: kind for kind in self._load_kinds(graph_config)}
        self.verify_kinds(kinds)

        edges = set()
        for kind in kinds.itervalues():
            for dep in kind.config.get('kind-dependencies', []):
                edges.add((kind.name, dep, 'kind-dependency'))
        kind_graph = Graph(set(kinds), edges)

        if self._target_kind:
            logger.info(
                "Limiting kinds to {target_kind} and dependencies".format(
                    target_kind=self._target_kind))
            kind_graph = kind_graph.transitive_closure({self._target_kind, 'docker-image'})

        logger.info("Generating full task set")
        all_tasks = {}
        for kind_name in kind_graph.visit_postorder():
            logger.debug("Loading tasks for kind {}".format(kind_name))
            kind = kinds[kind_name]
            try:
                new_tasks = kind.load_tasks(parameters, list(all_tasks.values()))
            except Exception:
                logger.exception("Error loading tasks for kind {}:".format(kind_name))
                raise
            for task in new_tasks:
                if task.label in all_tasks:
                    raise Exception("duplicate tasks with label " + task.label)
                all_tasks[task.label] = task
            logger.info("Generated {} tasks for kind {}".format(len(new_tasks), kind_name))
        full_task_set = TaskGraph(all_tasks, Graph(set(all_tasks), set()))
        self.verify_attributes(all_tasks)
        self.verify_run_using()
        yield verifications('full_task_set', full_task_set, graph_config)

        logger.info("Generating full task graph")
        edges = set()
        for t in full_task_set:
            for depname, dep in t.dependencies.iteritems():
                edges.add((t.label, dep, depname))

        full_task_graph = TaskGraph(all_tasks,
                                    Graph(full_task_set.graph.nodes, edges))
        logger.info("Full task graph contains %d tasks and %d dependencies" % (
            len(full_task_set.graph.nodes), len(edges)))
        yield verifications('full_task_graph', full_task_graph, graph_config)

        logger.info("Generating target task set")
        target_task_set = TaskGraph(dict(all_tasks),
                                    Graph(set(all_tasks.keys()), set()))
        for fltr in filters:
            old_len = len(target_task_set.graph.nodes)
            target_tasks = set(fltr(target_task_set, parameters, graph_config))
            target_task_set = TaskGraph(
                {l: all_tasks[l] for l in target_tasks},
                Graph(target_tasks, set()))
            logger.info('Filter %s pruned %d tasks (%d remain)' % (
                fltr.__name__,
                old_len - len(target_tasks),
                len(target_tasks)))

        yield verifications('target_task_set', target_task_set, graph_config)

        logger.info("Generating target task graph")
        # include all docker-image build tasks here, in case they are needed for a graph morph
        docker_image_tasks = set(t.label for t in full_task_graph.tasks.itervalues()
                                 if t.attributes['kind'] == 'docker-image')
        # include all tasks with `always_target` set
        always_target_tasks = set(t.label for t in full_task_graph.tasks.itervalues()
                                  if t.attributes.get('always_target'))
        logger.info('Adding %d tasks with `always_target` attribute' % (
                    len(always_target_tasks) - len(always_target_tasks & target_tasks)))
        target_graph = full_task_graph.graph.transitive_closure(
            target_tasks | docker_image_tasks | always_target_tasks)
        target_task_graph = TaskGraph(
            {l: all_tasks[l] for l in target_graph.nodes},
            target_graph)
        yield verifications('target_task_graph', target_task_graph, graph_config)

        logger.info("Generating optimized task graph")
        existing_tasks = parameters.get('existing_tasks')
        do_not_optimize = set(parameters.get('do_not_optimize', []))
        if not parameters.get('optimize_target_tasks', True):
            do_not_optimize = set(target_task_set.graph.nodes).union(do_not_optimize)

        # this is used for testing experimental optimization strategies
        strategies = os.environ.get('TASKGRAPH_OPTIMIZE_STRATEGIES')
        if strategies:
            strategies = find_object(strategies)

        optimized_task_graph, label_to_taskid = optimize_task_graph(
            target_task_graph,
            parameters,
            do_not_optimize,
            existing_tasks=existing_tasks,
            strategies=strategies,
        )

        yield verifications('optimized_task_graph', optimized_task_graph, graph_config)

        morphed_task_graph, label_to_taskid = morph(
            optimized_task_graph, label_to_taskid, parameters)

        yield 'label_to_taskid', label_to_taskid
        yield verifications('morphed_task_graph', morphed_task_graph, graph_config)

    def _run_until(self, name):
        while name not in self._run_results:
            try:
                k, v = self._run.next()
            except StopIteration:
                raise AttributeError("No such run result {}".format(name))
            self._run_results[k] = v
        return self._run_results[name]

    def verify_parameters(self, parameters):
        if not parameters.strict:
            return

        parameters_dict = dict(**parameters)
        verify_docs(
            filename="parameters.rst",
            identifiers=parameters_dict.keys(),
            appearing_as="inline-literal"
         )

    def verify_kinds(self, kinds):
        verify_docs(
            filename="kinds.rst",
            identifiers=kinds.keys(),
            appearing_as="heading"
         )

    def verify_attributes(self, all_tasks):
        attribute_set = set()
        for label, task in all_tasks.iteritems():
            attribute_set.update(task.attributes.keys())
        verify_docs(
            filename="attributes.rst",
            identifiers=list(attribute_set),
            appearing_as="heading"
         )

    def verify_run_using(self):
        from .transforms.job import registry
        verify_docs(
            filename="transforms.rst",
            identifiers=registry.keys(),
            appearing_as="inline-literal"
         )


def load_tasks_for_kind(parameters, kind, root_dir=None):
    """
    Get all the tasks of a given kind.

    This function is designed to be called from outside of taskgraph.
    """
    tgg = TaskGraphGenerator(root_dir=root_dir, parameters=parameters, target_kind=kind)
    return {
        task.task['metadata']['name']: task
        for task in tgg.full_task_set
        if task.kind == kind
    }
