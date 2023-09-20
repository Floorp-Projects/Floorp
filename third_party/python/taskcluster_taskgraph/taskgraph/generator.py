# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import logging
import os
from dataclasses import dataclass
from typing import Dict

from . import filter_tasks
from .config import GraphConfig, load_graph_config
from .graph import Graph
from .morph import morph
from .optimize.base import optimize_task_graph
from .parameters import parameters_loader
from .task import Task
from .taskgraph import TaskGraph
from .transforms.base import TransformConfig, TransformSequence
from .util.python_path import find_object
from .util.verify import verifications
from .util.yaml import load_yaml

logger = logging.getLogger(__name__)


class KindNotFound(Exception):
    """
    Raised when trying to load kind from a directory without a kind.yml.
    """


@dataclass(frozen=True)
class Kind:
    name: str
    path: str
    config: Dict
    graph_config: GraphConfig

    def _get_loader(self):
        try:
            loader = self.config["loader"]
        except KeyError:
            loader = "taskgraph.loader.default:loader"
        return find_object(loader)

    def load_tasks(self, parameters, loaded_tasks, write_artifacts):
        loader = self._get_loader()
        config = copy.deepcopy(self.config)

        kind_dependencies = config.get("kind-dependencies", [])
        kind_dependencies_tasks = {
            task.label: task for task in loaded_tasks if task.kind in kind_dependencies
        }

        inputs = loader(self.name, self.path, config, parameters, loaded_tasks)

        transforms = TransformSequence()
        for xform_path in config["transforms"]:
            if ":" not in xform_path:
                xform_path = f"{xform_path}:transforms"

            transform = find_object(xform_path)
            transforms.add(transform)

        # perform the transformations on the loaded inputs
        trans_config = TransformConfig(
            self.name,
            self.path,
            config,
            parameters,
            kind_dependencies_tasks,
            self.graph_config,
            write_artifacts=write_artifacts,
        )
        tasks = [
            Task(
                self.name,
                label=task_dict["label"],
                description=task_dict["description"],
                attributes=task_dict["attributes"],
                task=task_dict["task"],
                optimization=task_dict.get("optimization"),
                dependencies=task_dict.get("dependencies"),
                soft_dependencies=task_dict.get("soft-dependencies"),
                if_dependencies=task_dict.get("if-dependencies"),
            )
            for task_dict in transforms(trans_config, inputs)
        ]
        return tasks

    @classmethod
    def load(cls, root_dir, graph_config, kind_name):
        path = os.path.join(root_dir, kind_name)
        kind_yml = os.path.join(path, "kind.yml")
        if not os.path.exists(kind_yml):
            raise KindNotFound(kind_yml)

        logger.debug(f"loading kind `{kind_name}` from `{path}`")
        config = load_yaml(kind_yml)

        return cls(kind_name, path, config, graph_config)


class TaskGraphGenerator:
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

    def __init__(
        self,
        root_dir,
        parameters,
        decision_task_id="DECISION-TASK",
        write_artifacts=False,
    ):
        """
        @param root_dir: root directory, with subdirectories for each kind
        @param parameters: parameters for this task-graph generation, or callable
            taking a `GraphConfig` and returning parameters
        @type parameters: Union[Parameters, Callable[[GraphConfig], Parameters]]
        """
        if root_dir is None:
            root_dir = "taskcluster/ci"
        self.root_dir = root_dir
        self._parameters = parameters
        self._decision_task_id = decision_task_id
        self._write_artifacts = write_artifacts

        # start the generator
        self._run = self._run()
        self._run_results = {}

    @property
    def parameters(self):
        """
        The properties used for this graph.

        @type: Properties
        """
        return self._run_until("parameters")

    @property
    def full_task_set(self):
        """
        The full task set: all tasks defined by any kind (a graph without edges)

        @type: TaskGraph
        """
        return self._run_until("full_task_set")

    @property
    def full_task_graph(self):
        """
        The full task graph: the full task set, with edges representing
        dependencies.

        @type: TaskGraph
        """
        return self._run_until("full_task_graph")

    @property
    def target_task_set(self):
        """
        The set of targeted tasks (a graph without edges)

        @type: TaskGraph
        """
        return self._run_until("target_task_set")

    @property
    def target_task_graph(self):
        """
        The set of targeted tasks and all of their dependencies

        @type: TaskGraph
        """
        return self._run_until("target_task_graph")

    @property
    def optimized_task_graph(self):
        """
        The set of targeted tasks and all of their dependencies; tasks that
        have been optimized out are either omitted or replaced with a Task
        instance containing only a task_id.

        @type: TaskGraph
        """
        return self._run_until("optimized_task_graph")

    @property
    def label_to_taskid(self):
        """
        A dictionary mapping task label to assigned taskId.  This property helps
        in interpreting `optimized_task_graph`.

        @type: dictionary
        """
        return self._run_until("label_to_taskid")

    @property
    def morphed_task_graph(self):
        """
        The optimized task graph, with any subsequent morphs applied. This graph
        will have the same meaning as the optimized task graph, but be in a form
        more palatable to TaskCluster.

        @type: TaskGraph
        """
        return self._run_until("morphed_task_graph")

    @property
    def graph_config(self):
        """
        The configuration for this graph.

        @type: TaskGraph
        """
        return self._run_until("graph_config")

    def _load_kinds(self, graph_config, target_kinds=None):
        if target_kinds:
            # docker-image is an implicit dependency that never appears in
            # kind-dependencies.
            queue = target_kinds + ["docker-image"]
            seen_kinds = set()
            while queue:
                kind_name = queue.pop()
                if kind_name in seen_kinds:
                    continue
                seen_kinds.add(kind_name)
                kind = Kind.load(self.root_dir, graph_config, kind_name)
                yield kind
                queue.extend(kind.config.get("kind-dependencies", []))
        else:
            for kind_name in os.listdir(self.root_dir):
                try:
                    yield Kind.load(self.root_dir, graph_config, kind_name)
                except KindNotFound:
                    continue

    def _run(self):
        logger.info("Loading graph configuration.")
        graph_config = load_graph_config(self.root_dir)

        yield ("graph_config", graph_config)

        graph_config.register()

        # Initial verifications that don't depend on any generation state.
        verifications("initial")

        if callable(self._parameters):
            parameters = self._parameters(graph_config)
        else:
            parameters = self._parameters

        logger.info(f"Using {parameters}")
        logger.debug(f"Dumping parameters:\n{repr(parameters)}")

        filters = parameters.get("filters", [])
        # Always add legacy target tasks method until we deprecate that API.
        if "target_tasks_method" not in filters:
            filters.insert(0, "target_tasks_method")
        filters = [filter_tasks.filter_task_functions[f] for f in filters]

        yield self.verify("parameters", parameters)

        logger.info("Loading kinds")
        # put the kinds into a graph and sort topologically so that kinds are loaded
        # in post-order
        target_kinds = sorted(parameters.get("target-kinds", []))
        if target_kinds:
            logger.info(
                "Limiting kinds to following kinds and dependencies: {}".format(
                    ", ".join(target_kinds)
                )
            )
        kinds = {
            kind.name: kind for kind in self._load_kinds(graph_config, target_kinds)
        }
        verifications("kinds", kinds)

        edges = set()
        for kind in kinds.values():
            for dep in kind.config.get("kind-dependencies", []):
                edges.add((kind.name, dep, "kind-dependency"))
        kind_graph = Graph(set(kinds), edges)

        if target_kinds:
            kind_graph = kind_graph.transitive_closure(
                set(target_kinds) | {"docker-image"}
            )

        logger.info("Generating full task set")
        all_tasks = {}
        for kind_name in kind_graph.visit_postorder():
            logger.debug(f"Loading tasks for kind {kind_name}")
            kind = kinds[kind_name]
            try:
                new_tasks = kind.load_tasks(
                    parameters,
                    list(all_tasks.values()),
                    self._write_artifacts,
                )
            except Exception:
                logger.exception(f"Error loading tasks for kind {kind_name}:")
                raise
            for task in new_tasks:
                if task.label in all_tasks:
                    raise Exception("duplicate tasks with label " + task.label)
                all_tasks[task.label] = task
            logger.info(f"Generated {len(new_tasks)} tasks for kind {kind_name}")
        full_task_set = TaskGraph(all_tasks, Graph(set(all_tasks), set()))
        yield self.verify("full_task_set", full_task_set, graph_config, parameters)

        logger.info("Generating full task graph")
        edges = set()
        for t in full_task_set:
            for depname, dep in t.dependencies.items():
                if dep not in all_tasks.keys():
                    raise Exception(
                        f"Task '{t.label}' lists a dependency that does not exist: '{dep}'"
                    )
                edges.add((t.label, dep, depname))

        full_task_graph = TaskGraph(all_tasks, Graph(full_task_set.graph.nodes, edges))
        logger.info(
            "Full task graph contains %d tasks and %d dependencies"
            % (len(full_task_set.graph.nodes), len(edges))
        )
        yield self.verify("full_task_graph", full_task_graph, graph_config, parameters)

        logger.info("Generating target task set")
        target_task_set = TaskGraph(
            dict(all_tasks), Graph(set(all_tasks.keys()), set())
        )
        for fltr in filters:
            old_len = len(target_task_set.graph.nodes)
            target_tasks = set(fltr(target_task_set, parameters, graph_config))
            target_task_set = TaskGraph(
                {l: all_tasks[l] for l in target_tasks}, Graph(target_tasks, set())
            )
            logger.info(
                "Filter %s pruned %d tasks (%d remain)"
                % (fltr.__name__, old_len - len(target_tasks), len(target_tasks))
            )

        yield self.verify("target_task_set", target_task_set, graph_config, parameters)

        logger.info("Generating target task graph")
        # include all tasks with `always_target` set
        if parameters["enable_always_target"]:
            always_target_tasks = {
                t.label
                for t in full_task_graph.tasks.values()
                if t.attributes.get("always_target")
                if parameters["enable_always_target"] is True
                or t.kind in parameters["enable_always_target"]
            }
        else:
            always_target_tasks = set()
        logger.info(
            "Adding %d tasks with `always_target` attribute"
            % (len(always_target_tasks) - len(always_target_tasks & target_tasks))
        )
        requested_tasks = target_tasks | always_target_tasks
        target_graph = full_task_graph.graph.transitive_closure(requested_tasks)
        target_task_graph = TaskGraph(
            {l: all_tasks[l] for l in target_graph.nodes}, target_graph
        )
        yield self.verify(
            "target_task_graph", target_task_graph, graph_config, parameters
        )

        logger.info("Generating optimized task graph")
        existing_tasks = parameters.get("existing_tasks")
        do_not_optimize = set(parameters.get("do_not_optimize", []))
        if not parameters.get("optimize_target_tasks", True):
            do_not_optimize = set(target_task_set.graph.nodes).union(do_not_optimize)

        # this is used for testing experimental optimization strategies
        strategies = os.environ.get(
            "TASKGRAPH_OPTIMIZE_STRATEGIES", parameters.get("optimize_strategies")
        )
        if strategies:
            strategies = find_object(strategies)

        optimized_task_graph, label_to_taskid = optimize_task_graph(
            target_task_graph,
            requested_tasks,
            parameters,
            do_not_optimize,
            self._decision_task_id,
            existing_tasks=existing_tasks,
            strategy_override=strategies,
        )

        yield self.verify(
            "optimized_task_graph", optimized_task_graph, graph_config, parameters
        )

        morphed_task_graph, label_to_taskid = morph(
            optimized_task_graph, label_to_taskid, parameters, graph_config
        )

        yield "label_to_taskid", label_to_taskid
        yield self.verify(
            "morphed_task_graph", morphed_task_graph, graph_config, parameters
        )

    def _run_until(self, name):
        while name not in self._run_results:
            try:
                k, v = next(self._run)
            except StopIteration:
                raise AttributeError(f"No such run result {name}")
            self._run_results[k] = v
        return self._run_results[name]

    def verify(self, name, obj, *args, **kwargs):
        verifications(name, obj, *args, **kwargs)
        return name, obj


def load_tasks_for_kind(parameters, kind, root_dir=None):
    """
    Get all the tasks of a given kind.

    This function is designed to be called from outside of taskgraph.
    """
    # make parameters read-write
    parameters = dict(parameters)
    parameters["target-kinds"] = [kind]
    parameters = parameters_loader(spec=None, strict=False, overrides=parameters)
    tgg = TaskGraphGenerator(root_dir=root_dir, parameters=parameters)
    return {
        task.task["metadata"]["name"]: task
        for task in tgg.full_task_set
        if task.kind == kind
    }
