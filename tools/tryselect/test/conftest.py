# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from unittest.mock import MagicMock

import pytest
import yaml
from moztest.resolve import TestResolver
from taskgraph.graph import Graph
from taskgraph.task import Task
from taskgraph.taskgraph import TaskGraph
from tryselect import push


@pytest.fixture
def tg(request):
    if not hasattr(request.module, "TASKS"):
        pytest.fail(
            "'tg' fixture used from a module that didn't define the TASKS variable"
        )

    tasks = request.module.TASKS
    for task in tasks:
        task.setdefault("task", {})
        task["task"].setdefault("tags", {})

    tasks = {t["label"]: Task(**t) for t in tasks}
    return TaskGraph(tasks, Graph(tasks.keys(), set()))


@pytest.fixture
def patch_resolver(monkeypatch):
    def inner(suites, tests):
        def fake_test_metadata(*args, **kwargs):
            return suites, tests

        monkeypatch.setattr(TestResolver, "resolve_metadata", fake_test_metadata)

    return inner


@pytest.fixture(autouse=True)
def patch_vcs(monkeypatch):
    attrs = {
        "path": push.vcs.path,
    }
    mock = MagicMock()
    mock.configure_mock(**attrs)
    monkeypatch.setattr(push, "vcs", mock)


@pytest.fixture(scope="session")
def run_mach():
    import mach_initialize
    from tryselect.tasks import build

    mach = mach_initialize.initialize(build.topsrcdir)

    def inner(args):
        return mach.run(args)

    return inner


def pytest_generate_tests(metafunc):
    if all(
        fixture in metafunc.fixturenames
        for fixture in ("task_config", "args", "expected")
    ):

        def load_tests():
            for task_config, tests in metafunc.module.TASK_CONFIG_TESTS.items():
                for args, expected in tests:
                    yield (task_config, args, expected)

        tests = list(load_tests())
        ids = ["{} {}".format(t[0], " ".join(t[1])).strip() for t in tests]
        metafunc.parametrize("task_config,args,expected", tests, ids=ids)

    elif all(
        fixture in metafunc.fixturenames for fixture in ("shared_name", "shared_preset")
    ):
        preset_path = os.path.join(
            push.build.topsrcdir, "tools", "tryselect", "try_presets.yml"
        )
        with open(preset_path, "r") as fh:
            presets = list(yaml.safe_load(fh).items())

        ids = [p[0] for p in presets]

        # Mark fuzzy presets on Windows xfail due to fzf not being installed.
        if os.name == "nt":
            for i, preset in enumerate(presets):
                if preset[1]["selector"] == "fuzzy":
                    presets[i] = pytest.param(*preset, marks=pytest.mark.xfail)

        metafunc.parametrize("shared_name,shared_preset", presets, ids=ids)
