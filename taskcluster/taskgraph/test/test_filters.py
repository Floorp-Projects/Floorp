# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import shutil
import tempfile
import unittest

from mozunit import main

from .. import filter_tasks
from ..graph import (
    Graph,
)
from ..taskgraph import (
    TaskGraph,
)
from .util import (
    TestTask,
)


class TestServoFilter(unittest.TestCase):
    def setUp(self):
        self._tmpdir = tempfile.mkdtemp()
        filter_tasks.GECKO = self._tmpdir

    def tearDown(self):
        shutil.rmtree(self._tmpdir)

    def test_basic(self):
        graph = TaskGraph(tasks={
            'a': TestTask(kind='build', label='a',
                          attributes={'build_platform': 'linux64'}),
            'b': TestTask(kind='build', label='b',
                          attributes={'build_platform': 'linux64-stylo'}),
            'c': TestTask(kind='desktop-test', label='c', attributes={}),
        }, graph=Graph(nodes={'a', 'b', 'c'}, edges=set()))

        # Missing servo/ directory should prune tasks requiring Servo.
        self.assertEqual(set(filter_tasks.filter_servo(graph, {})), {'a', 'c'})

        # Servo tasks should be retained if servo/components/style/ present.
        os.makedirs(os.path.join(self._tmpdir, 'servo', 'components', 'style'))
        self.assertEqual(set(filter_tasks.filter_servo(graph, {})),
                         {'a', 'b', 'c'})


if __name__ == '__main__':
    main()
