# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from datetime import datetime
from time import mktime

import pytest
from mozunit import main

from taskgraph.util.backstop import is_backstop


@pytest.fixture(scope='module')
def params():
    return {
        'branch': 'integration/autoland',
        'head_repository': 'https://hg.mozilla.org/integration/autoland',
        'head_rev': 'abcdef',
        'project': 'autoland',
        'pushlog_id': 1,
        'pushdate': mktime(datetime.now().timetuple()),
    }


def test_is_backstop(responses, params):

    responses.add(
        responses.GET,
        "https://hg.mozilla.org/integration/autoland/json-pushes/?version=2&startID=16&endID=17",  # noqa
        json={"pushes": {"17": {}}},
        status=200,
    )

    # If there's no previous push date, run tasks
    params['pushlog_id'] = 18
    assert is_backstop(params)

    responses.add(
        responses.GET,
        "https://hg.mozilla.org/integration/autoland/json-pushes/?version=2&startID=17&endID=18",  # noqa
        json={"pushes": {"18": {"date": params['pushdate']}}},
        status=200,
    )

    # Only multiples of 20 schedule tasks. Pushdate from push 19 was cached.
    params['pushlog_id'] = 19
    params['pushdate'] += 3599
    assert not is_backstop(params)

    params['pushlog_id'] = 20
    params['pushdate'] += 1
    assert is_backstop(params)

    responses.add(
        responses.GET,
        "https://hg.mozilla.org/integration/autoland/json-pushes/?version=2&startID=19&endID=20",  # noqa
        json={"pushes": {"20": {"date": params['pushdate']}}},
        status=200,
    )

    # Tasks are also scheduled if four hours have passed.
    params['pushlog_id'] = 21
    params['pushdate'] += 4 * 3600
    assert is_backstop(params)


if __name__ == '__main__':
    main()
