# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import os

import pytest
from mach.logging import LoggingManager
from responses import RequestsMock
from taskgraph import target_tasks as target_tasks_mod
from taskgraph.config import GraphConfig, load_graph_config
from taskgraph.optimize import base as optimize_mod
from taskgraph.optimize.base import OptimizationStrategy
from taskgraph.parameters import Parameters

from gecko_taskgraph import (
    GECKO,
    generator,
)
from gecko_taskgraph.actions import render_actions_json
from gecko_taskgraph.generator import TaskGraphGenerator, Kind
from gecko_taskgraph.util.templates import merge


@pytest.fixture
def responses():
    with RequestsMock() as rsps:
        yield rsps


@pytest.fixture(scope="session", autouse=True)
def patch_prefherder(request):
    from _pytest.monkeypatch import MonkeyPatch

    m = MonkeyPatch()
    m.setattr(
        "gecko_taskgraph.util.bugbug._write_perfherder_data",
        lambda lower_is_better: None,
    )
    yield
    m.undo()


@pytest.fixture(scope="session", autouse=True)
def enable_logging():
    """Ensure logs from gecko_taskgraph are displayed when a test fails."""
    lm = LoggingManager()
    lm.add_terminal_logging()


@pytest.fixture(scope="session")
def graph_config():
    return load_graph_config(os.path.join(GECKO, "taskcluster", "ci"))


@pytest.fixture(scope="session")
def actions_json(graph_config):
    decision_task_id = "abcdef"
    return render_actions_json(Parameters(strict=False), graph_config, decision_task_id)


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

    @staticmethod
    def create(name, extra_config, graph_config):
        config = {
            "transforms": [],
        }
        if extra_config:
            config.update(extra_config)
        return FakeKind(name, "/fake", config, graph_config)


class WithFakeKind(TaskGraphGenerator):
    def _load_kinds(self, graph_config, target_kind=None):
        for kind_name, cfg in self.parameters["_kinds"]:
            yield FakeKind.create(kind_name, cfg, graph_config)


def fake_load_graph_config(root_dir):
    graph_config = GraphConfig(
        {"trust-domain": "test-domain", "taskgraph": {}}, root_dir
    )
    graph_config.__dict__["register"] = lambda: None
    return graph_config


class FakeParameters(dict):
    strict = True

    def file_url(self, path, pretty=False):
        return ""


class FakeOptimization(OptimizationStrategy):
    description = "Fake strategy for testing"

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
        FakeKind.loaded_kinds = loaded_kinds = []
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

        tgg = WithFakeKind("/root", parameters)
        tgg.loaded_kinds = loaded_kinds
        return tgg

    return inner


@pytest.fixture
def run_transform():

    graph_config = fake_load_graph_config("/root")
    kind = FakeKind.create("fake", {}, graph_config)

    def inner(xform, tasks):
        if isinstance(tasks, dict):
            tasks = [tasks]
        return xform(kind.config, tasks)

    return inner
