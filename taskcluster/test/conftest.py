# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import json
import os

import pytest
from mach.logging import LoggingManager
from responses import RequestsMock

from taskgraph.generator import TaskGraphGenerator
from taskgraph.parameters import parameters_loader
from taskgraph.util.bugbug import BUGBUG_BASE_URL

here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture(scope="session")
def responses():
    with RequestsMock() as rsps:
        yield rsps


@pytest.fixture(scope="session")
def datadir():
    return os.path.join(here, "data")


@pytest.fixture(scope="session")
def create_tgg(responses, datadir):

    # Setup logging.
    lm = LoggingManager()
    lm.add_terminal_logging()

    def inner(parameters=None, overrides=None):
        params = parameters_loader(parameters, strict=False, overrides=overrides)
        tgg = TaskGraphGenerator(None, params)

        # Mock out certain requests as they may depend on a revision that does
        # not exist on hg.mozilla.org.
        mock_requests = {}

        # bugbug /push/schedules
        url = BUGBUG_BASE_URL + "/push/{project}/{head_rev}/schedules".format(**tgg.parameters)
        mock_requests[url] = "bugbug-push-schedules.json"

        # files changed
        url = "{head_repository}/json-automationrelevance/{head_rev}".format(**tgg.parameters)
        mock_requests[url] = "automationrelevance.json"

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


@pytest.fixture(scope="session")
def filter_tasks():

    def inner(graph, func):
        return filter(func, graph.tasks.values())

    return inner
