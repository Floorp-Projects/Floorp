# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from datetime import datetime
from textwrap import dedent
from time import mktime

import pytest
from mozunit import main
from taskgraph.util.taskcluster import get_artifact_url, get_index_url, get_task_url

from gecko_taskgraph.util.backstop import (
    BACKSTOP_INDEX,
    BACKSTOP_PUSH_INTERVAL,
    BACKSTOP_TIME_INTERVAL,
    is_backstop,
)

LAST_BACKSTOP_ID = 0
LAST_BACKSTOP_PUSHDATE = mktime(datetime.now().timetuple())
DEFAULT_RESPONSES = {
    "index": {
        "status": 200,
        "json": {"taskId": LAST_BACKSTOP_ID},
    },
    "artifact": {
        "status": 200,
        "body": dedent(
            """
            pushdate: {}
        """.format(
                LAST_BACKSTOP_PUSHDATE
            )
        ),
    },
    "status": {
        "status": 200,
        "json": {"status": {"state": "complete"}},
    },
}


@pytest.fixture
def params():
    return {
        "branch": "integration/autoland",
        "head_repository": "https://hg.mozilla.org/integration/autoland",
        "head_rev": "abcdef",
        "project": "autoland",
        "pushdate": LAST_BACKSTOP_PUSHDATE + 1,
        "pushlog_id": LAST_BACKSTOP_ID + 1,
    }


@pytest.mark.parametrize(
    "response_args,extra_params,expected",
    (
        pytest.param(
            {
                "index": {"status": 404},
            },
            {"pushlog_id": 1},
            True,
            id="no previous backstop",
        ),
        pytest.param(
            {
                "index": DEFAULT_RESPONSES["index"],
                "status": DEFAULT_RESPONSES["status"],
                "artifact": {"status": 404},
            },
            {"pushlog_id": 1},
            False,
            id="previous backstop not finished",
        ),
        pytest.param(
            DEFAULT_RESPONSES,
            {
                "pushlog_id": LAST_BACKSTOP_ID + 1,
                "pushdate": LAST_BACKSTOP_PUSHDATE + 1,
            },
            False,
            id="not a backstop",
        ),
        pytest.param(
            {},
            {
                "pushlog_id": BACKSTOP_PUSH_INTERVAL,
            },
            True,
            id="backstop interval",
        ),
        pytest.param(
            DEFAULT_RESPONSES,
            {
                "pushdate": LAST_BACKSTOP_PUSHDATE + (BACKSTOP_TIME_INTERVAL * 60),
            },
            True,
            id="time elapsed",
        ),
        pytest.param(
            {},
            {
                "project": "try",
                "pushlog_id": BACKSTOP_PUSH_INTERVAL,
            },
            False,
            id="try not a backstop",
        ),
        pytest.param(
            {},
            {
                "project": "mozilla-central",
            },
            True,
            id="release branches always a backstop",
        ),
        pytest.param(
            {
                "index": DEFAULT_RESPONSES["index"],
                "status": {
                    "status": 200,
                    "json": {"status": {"state": "failed"}},
                },
            },
            {},
            True,
            id="last backstop failed",
        ),
    ),
)
def test_is_backstop(responses, params, response_args, extra_params, expected):
    urls = {
        "index": get_index_url(
            BACKSTOP_INDEX.format(
                **{"trust-domain": "gecko", "project": params["project"]}
            )
        ),
        "artifact": get_artifact_url(LAST_BACKSTOP_ID, "public/parameters.yml"),
        "status": get_task_url(LAST_BACKSTOP_ID) + "/status",
    }

    for key in ("index", "status", "artifact"):
        if key in response_args:
            print(urls[key])
            responses.add(responses.GET, urls[key], **response_args[key])

    params.update(extra_params)
    assert is_backstop(params) is expected


if __name__ == "__main__":
    main()
