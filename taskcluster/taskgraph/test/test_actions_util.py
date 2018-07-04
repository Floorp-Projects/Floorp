# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
from mozunit import main
from taskgraph.actions.util import (
    relativize_datestamps
)

TASK_DEF = {
    'created': '2017-10-10T18:33:03.460Z',
    # note that this is not an even number of seconds off!
    'deadline': '2017-10-11T18:33:03.461Z',
    'dependencies': [],
    'expires': '2018-10-10T18:33:04.461Z',
    'payload': {
        'artifacts': {
            'public': {
                'expires': '2018-10-10T18:33:03.463Z',
                'path': '/builds/worker/artifacts',
                'type': 'directory',
            },
        },
        'maxRunTime': 1800,
    },
}


class TestRelativize(unittest.TestCase):

    def test_relativize(self):
        rel = relativize_datestamps(TASK_DEF)
        import pprint
        pprint.pprint(rel)
        assert rel['created'] == {'relative-datestamp': '0 seconds'}
        assert rel['deadline'] == {'relative-datestamp': '86400 seconds'}
        assert rel['expires'] == {'relative-datestamp': '31536001 seconds'}
        assert rel['payload']['artifacts']['public']['expires'] == \
            {'relative-datestamp': '31536000 seconds'}


if __name__ == '__main__':
    main()
