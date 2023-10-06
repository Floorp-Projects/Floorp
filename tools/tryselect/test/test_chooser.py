# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import multiprocessing

import mozunit
import pytest
from tryselect.selectors.chooser.app import create_application

TASKS = [
    {
        "kind": "build",
        "label": "build-windows",
        "attributes": {
            "build_platform": "windows",
        },
    },
    {
        "kind": "test",
        "label": "test-windows-mochitest-e10s",
        "attributes": {
            "unittest_suite": "mochitest-browser-chrome",
            "mochitest_try_name": "mochitest-browser-chrome",
        },
    },
]


@pytest.fixture
def queue():
    return multiprocessing.Queue()


@pytest.fixture
def app(tg, queue):
    app = create_application(tg, queue)
    app.config["TESTING"] = True

    ctx = app.app_context()
    ctx.push()
    yield app
    ctx.pop()


def test_try_chooser(app, queue: multiprocessing.Queue):
    client = app.test_client()

    response = client.get("/")
    assert response.status_code == 200

    expected_output = [
        b"""<title>Try Chooser Enhanced</title>""",
        b"""<input class="filter" type="checkbox" id=windows name="build" value='{"build_platform": ["windows"]}' onchange="console.log('checkbox onchange triggered');apply();">""",  # noqa
        b"""<input class="filter" type="checkbox" id=mochitest-browser-chrome name="test" value='{"unittest_suite": ["mochitest-browser-chrome"]}' onchange="console.log('checkbox onchange triggered');apply();">""",  # noqa
    ]

    for expected in expected_output:
        assert expected in response.data

    response = client.post("/", data={"action": "Cancel"})
    assert response.status_code == 200
    assert b"You may now close this page" in response.data
    assert queue.get() == []

    response = client.post("/", data={"action": "Push", "selected-tasks": ""})
    assert response.status_code == 200
    assert b"You may now close this page" in response.data
    assert queue.get() == []

    response = client.post(
        "/",
        data={
            "action": "Push",
            "selected-tasks": "build-windows\ntest-windows-mochitest-e10s",
        },
    )
    assert response.status_code == 200
    assert b"You may now close this page" in response.data
    assert set(queue.get()) == set(["build-windows", "test-windows-mochitest-e10s"])


if __name__ == "__main__":
    mozunit.main()
