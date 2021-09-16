# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import pytest
from mozunit import main

from taskgraph.generator import TaskGraphGenerator, Kind, load_tasks_for_kind
from taskgraph.optimize import OptimizationStrategy
from taskgraph.config import GraphConfig
from taskgraph.util.templates import merge
from taskgraph import (
    generator,
    graph,
    optimize as optimize_mod,
    target_tasks as target_tasks_mod,
)


def fake_loader(kind, path, config, parameters, loaded_tasks):
    for i in range(3):
        dependencies = {}
        if i >= 1:
            dependencies["prev"] = f"{kind}-t-{i - 1}"

        task = {
            "kind": kind,
            "label": f"{kind}-t-{i}",
            "description": f"{kind} task {i}",
            "attributes": {"_tasknum": str(i)},
            "task": {
                "i": i,
                "metadata": {"name": f"t-{i}"},
                "deadline": "soon",
            },
            "dependencies": dependencies,
        }
        if "job-defaults" in config:
            task = merge(config["job-defaults"], task)
        yield task


class FakeKind(Kind):
    def _get_loader(self):
        return fake_loader

    def load_tasks(self, parameters, loaded_tasks, write_artifacts):
        FakeKind.loaded_kinds.append(self.name)
        return super().load_tasks(parameters, loaded_tasks, write_artifacts)


class WithFakeKind(TaskGraphGenerator):
    def _load_kinds(self, graph_config, target_kind=None):
        for kind_name, cfg in self.parameters["_kinds"]:
            config = {
                "transforms": [],
            }
            if cfg:
                config.update(cfg)
            yield FakeKind(kind_name, "/fake", config, graph_config)


def fake_load_graph_config(root_dir):
    graph_config = GraphConfig(
        {"trust-domain": "test-domain", "taskgraph": {}}, root_dir
    )
    graph_config.__dict__["register"] = lambda: None
    return graph_config


class FakeParameters(dict):
    strict = True


class FakeOptimization(OptimizationStrategy):
    def __init__(self, mode, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.mode = mode

    def should_remove_task(self, task, params, arg):
        if self.mode == "always":
            return True
        if self.mode == "even":
            return task.task["i"] % 2 == 0
        if self.mode == "odd":
            return task.task["i"] % 2 != 0
        return False


@pytest.fixture
def maketgg(monkeypatch):
    def inner(target_tasks=None, kinds=[("_fake", [])], params=None):
        params = params or {}
        FakeKind.loaded_kinds = []
        target_tasks = target_tasks or []

        def target_tasks_method(full_task_graph, parameters, graph_config):
            return target_tasks

        fake_registry = {
            mode: FakeOptimization(mode) for mode in ("always", "never", "even", "odd")
        }

        target_tasks_mod._target_task_methods["test_method"] = target_tasks_method
        monkeypatch.setattr(optimize_mod, "registry", fake_registry)

        parameters = FakeParameters(
            {
                "_kinds": kinds,
                "backstop": False,
                "target_tasks_method": "test_method",
                "test_manifest_loader": "default",
                "try_mode": None,
                "try_task_config": {},
                "tasks_for": "hg-push",
                "project": "mozilla-central",
            }
        )
        parameters.update(params)

        monkeypatch.setattr(generator, "load_graph_config", fake_load_graph_config)

        return WithFakeKind("/root", parameters)

    return inner


def test_kind_ordering(maketgg):
    "When task kinds depend on each other, they are loaded in postorder"
    tgg = maketgg(
        kinds=[
            ("_fake3", {"kind-dependencies": ["_fake2", "_fake1"]}),
            ("_fake2", {"kind-dependencies": ["_fake1"]}),
            ("_fake1", {"kind-dependencies": []}),
        ]
    )
    tgg._run_until("full_task_set")
    assert FakeKind.loaded_kinds == ["_fake1", "_fake2", "_fake3"]


def test_full_task_set(maketgg):
    "The full_task_set property has all tasks"
    tgg = maketgg()
    assert tgg.full_task_set.graph == graph.Graph(
        {"_fake-t-0", "_fake-t-1", "_fake-t-2"}, set()
    )
    assert sorted(tgg.full_task_set.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_fake-t-2"]
    )


def test_full_task_graph(maketgg):
    "The full_task_graph property has all tasks, and links"
    tgg = maketgg()
    assert tgg.full_task_graph.graph == graph.Graph(
        {"_fake-t-0", "_fake-t-1", "_fake-t-2"},
        {
            ("_fake-t-1", "_fake-t-0", "prev"),
            ("_fake-t-2", "_fake-t-1", "prev"),
        },
    )
    assert sorted(tgg.full_task_graph.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_fake-t-2"]
    )


def test_target_task_set(maketgg):
    "The target_task_set property has the targeted tasks"
    tgg = maketgg(["_fake-t-1"])
    assert tgg.target_task_set.graph == graph.Graph({"_fake-t-1"}, set())
    assert set(tgg.target_task_set.tasks.keys()) == {"_fake-t-1"}


def test_target_task_graph(maketgg):
    "The target_task_graph property has the targeted tasks and deps"
    tgg = maketgg(["_fake-t-1"])
    assert tgg.target_task_graph.graph == graph.Graph(
        {"_fake-t-0", "_fake-t-1"}, {("_fake-t-1", "_fake-t-0", "prev")}
    )
    assert sorted(tgg.target_task_graph.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1"]
    )


def test_always_target_tasks(maketgg):
    "The target_task_graph includes tasks with 'always_target'"
    tgg_args = {
        "target_tasks": ["_fake-t-0", "_fake-t-1", "_ignore-t-0", "_ignore-t-1"],
        "kinds": [
            ("_fake", {"job-defaults": {"optimization": {"odd": None}}}),
            (
                "_ignore",
                {
                    "job-defaults": {
                        "attributes": {"always_target": True},
                        "optimization": {"even": None},
                    }
                },
            ),
        ],
        "params": {"optimize_target_tasks": False},
    }
    tgg = maketgg(**tgg_args)
    assert sorted(tgg.target_task_set.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_ignore-t-0", "_ignore-t-1"]
    )
    assert sorted(tgg.target_task_graph.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_ignore-t-0", "_ignore-t-1", "_ignore-t-2"]
    )
    assert sorted(t.label for t in tgg.optimized_task_graph.tasks.values()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_ignore-t-0", "_ignore-t-1"]
    )


def test_optimized_task_graph(maketgg):
    "The optimized task graph contains task ids"
    tgg = maketgg(["_fake-t-2"])
    tid = tgg.label_to_taskid
    assert tgg.optimized_task_graph.graph == graph.Graph(
        {tid["_fake-t-0"], tid["_fake-t-1"], tid["_fake-t-2"]},
        {
            (tid["_fake-t-1"], tid["_fake-t-0"], "prev"),
            (tid["_fake-t-2"], tid["_fake-t-1"], "prev"),
        },
    )


def test_load_tasks_for_kind(monkeypatch):
    """
    `load_tasks_for_kinds` will load the tasks for the provided kind
    """
    monkeypatch.setattr(generator, "TaskGraphGenerator", WithFakeKind)
    monkeypatch.setattr(generator, "load_graph_config", fake_load_graph_config)

    tasks = load_tasks_for_kind(
        {"_kinds": [("_example-kind", []), ("docker-image", [])]},
        "_example-kind",
        "/root",
    )
    assert "t-1" in tasks and tasks["t-1"].label == "_example-kind-t-1"


if __name__ == "__main__":
    main()
