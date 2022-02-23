# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import json
import logging
import os

import pytest
from responses import RequestsMock, logger as rsps_logger
from taskgraph.parameters import parameters_loader

from gecko_taskgraph.generator import TaskGraphGenerator
from gecko_taskgraph.util.hg import PUSHLOG_PUSHES_TMPL
from gecko_taskgraph.util.bugbug import BUGBUG_BASE_URL

here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture(scope="session")
def responses():
    rsps_logger.setLevel(logging.WARNING)
    with RequestsMock(assert_all_requests_are_fired=False) as rsps:
        yield rsps


@pytest.fixture(scope="session")
def datadir():
    return os.path.join(here, "data")


@pytest.fixture(scope="session")
def create_tgg(responses, datadir):
    def inner(parameters=None, overrides=None):
        params = parameters_loader(parameters, strict=False, overrides=overrides)
        tgg = TaskGraphGenerator(None, params)

        # Mock out certain requests as they may depend on a revision that does
        # not exist on hg.mozilla.org.
        mock_requests = {}

        # bugbug /push/schedules
        url = BUGBUG_BASE_URL + "/push/{project}/{head_rev}/schedules".format(
            **tgg.parameters
        )
        mock_requests[url] = "bugbug-push-schedules.json"

        # files changed
        url = "{head_repository}/json-automationrelevance/{head_rev}".format(
            **tgg.parameters
        )
        mock_requests[url] = "automationrelevance.json"

        url = PUSHLOG_PUSHES_TMPL.format(
            repository=tgg.parameters["head_repository"],
            push_id_start=int(tgg.parameters["pushlog_id"]) - 2,
            push_id_end=int(tgg.parameters["pushlog_id"]) - 1,
        )
        mock_requests[url] = "pushes.json"

        for url, filename in mock_requests.items():
            with open(os.path.join(datadir, filename)) as fh:
                responses.add(
                    responses.GET,
                    url,
                    json=json.load(fh),
                    status=200,
                )

        # Still allow other real requests.
        responses.add_passthru("https://hg.mozilla.org")
        responses.add_passthru("https://firefox-ci-tc.services.mozilla.com")
        return tgg

    return inner


@pytest.fixture(scope="module")
def tgg(request, create_tgg):
    if not hasattr(request.module, "PARAMS"):
        pytest.fail("'tgg' fixture requires a module-level 'PARAMS' variable")

    tgg = create_tgg(overrides=request.module.PARAMS)
    return tgg


@pytest.fixture(scope="module")
def params(tgg):
    return tgg.parameters


@pytest.fixture(scope="module")
def full_task_graph(tgg):
    return tgg.full_task_graph


@pytest.fixture(scope="module")
def optimized_task_graph(full_task_graph, tgg):
    return tgg.optimized_task_graph


@pytest.fixture(scope="session")
def filter_tasks():
    def inner(graph, func):
        return filter(func, graph.tasks.values())

    return inner


@pytest.fixture(scope="session")
def print_dependents():
    def inner(graph, label, indent=""):
        if indent == "":
            print(f"Dependent graph for {label}:")

        dependents = set()
        for task in graph.tasks.values():
            if label in task.dependencies.values():
                dependents.add(task.label)

        print(f"{indent}{label}")
        if dependents:
            for dep in sorted(dependents):
                inner(graph, dep, indent=indent + "  ")

    return inner
