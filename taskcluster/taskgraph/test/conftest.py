# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import os

import pytest
from mach.logging import LoggingManager
from responses import RequestsMock

from taskgraph import GECKO
from taskgraph.actions import render_actions_json
from taskgraph.config import load_graph_config
from taskgraph.parameters import Parameters


@pytest.fixture
def responses():
    with RequestsMock() as rsps:
        yield rsps


@pytest.fixture(scope="session", autouse=True)
def patch_prefherder(request):
    from _pytest.monkeypatch import MonkeyPatch

    m = MonkeyPatch()
    m.setattr(
        "taskgraph.util.bugbug._write_perfherder_data",
        lambda lower_is_better: None,
    )
    yield
    m.undo()


@pytest.fixture(scope="session", autouse=True)
def enable_logging():
    """Ensure logs from taskgraph are displayed when a test fails."""
    lm = LoggingManager()
    lm.add_terminal_logging()


@pytest.fixture(scope="session")
def graph_config():
    return load_graph_config(os.path.join(GECKO, "taskcluster", "ci"))


@pytest.fixture(scope="session")
def actions_json(graph_config):
    decision_task_id = "abcdef"
    return render_actions_json(Parameters(strict=False), graph_config, decision_task_id)
