# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

from mach.logging import LoggingManager

import pytest
from responses import RequestsMock


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
