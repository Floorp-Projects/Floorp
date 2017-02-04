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

        shared = os.path.join(self._tmpdir, 'toolkit', 'library', 'rust', 'shared')
        cargo = os.path.join(shared, 'Cargo.toml')
        os.makedirs(shared)
        with open(cargo, 'a'):
            pass

        # Default Cargo.toml should result in Servo tasks being pruned.
        self.assertEqual(set(filter_tasks.filter_servo(graph, {})), {'a', 'c'})

        # Servo tasks should be retained if real geckolib is present.
        with open(cargo, 'wb') as fh:
            fh.write(b'[dependencies]\n')
            fh.write(b'geckoservo = { path = "../../../../servo/ports/geckolib" }\n')

        self.assertEqual(set(filter_tasks.filter_servo(graph, {})),
                         {'a', 'b', 'c'})


if __name__ == '__main__':
    main()
