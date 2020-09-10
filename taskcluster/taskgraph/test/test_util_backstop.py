# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from datetime import datetime
from textwrap import dedent
from time import mktime

import pytest
from mozunit import main

from taskgraph.util.backstop import (
    is_backstop,
    BACKSTOP_INDEX,
    BACKSTOP_PUSH_INTERVAL,
    BACKSTOP_TIME_INTERVAL,
)
from taskgraph.util.taskcluster import get_index_url


@pytest.fixture(scope='module')
def params():
    return {
        'branch': 'integration/autoland',
        'head_repository': 'https://hg.mozilla.org/integration/autoland',
        'head_rev': 'abcdef',
        'project': 'autoland',
        'pushdate': mktime(datetime.now().timetuple()),
    }


def test_is_backstop(responses, params):
    url = get_index_url(
        BACKSTOP_INDEX.format(project=params["project"])
    ) + "/artifacts/public/parameters.yml"

    responses.add(
        responses.GET,
        url,
        status=404,
    )

    # If there's no previous push date, run tasks
    params["pushlog_id"] = 1
    assert is_backstop(params)

    responses.replace(
        responses.GET,
        url,
        body=dedent("""
        pushdate: {pushdate}
        """.format(pushdate=params["pushdate"])),
        status=200,
    )

    # Only multiples of push interval schedule tasks.
    params['pushlog_id'] = BACKSTOP_PUSH_INTERVAL - 1
    params['pushdate'] += 1
    assert not is_backstop(params)

    params['pushlog_id'] = BACKSTOP_PUSH_INTERVAL
    params['pushdate'] += 1
    assert is_backstop(params)

    # Tasks are also scheduled if the time interval has passed.
    params['pushlog_id'] = BACKSTOP_PUSH_INTERVAL + 1
    params['pushdate'] += BACKSTOP_TIME_INTERVAL * 60
    assert is_backstop(params)

    # Try is never considered a backstop.
    params['project'] = 'try'
    assert not is_backstop(params)

    # Non autoland branches are always considered a backstop.
    params['project'] = 'mozilla-central'
    params['pushdate'] -= BACKSTOP_TIME_INTERVAL * 60
    assert is_backstop(params)


if __name__ == '__main__':
    main()
