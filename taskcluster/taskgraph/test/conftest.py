# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

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
        "taskgraph.util.bugbug._write_perfherder_data", lambda lower_is_better: None,
    )
    yield
    m.undo()
