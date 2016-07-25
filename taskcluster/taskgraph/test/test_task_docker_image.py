# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
import os

from ..task import docker_image
from mozunit import main


KIND_PATH = os.path.join(docker_image.GECKO, 'taskcluster', 'ci', 'docker-image')


class TestDockerImageKind(unittest.TestCase):

    def setUp(self):
        self.task = docker_image.DockerImageTask(
            'docker-image',
            KIND_PATH,
            {},
            {},
            index_paths=[])

    def test_get_task_dependencies(self):
        # this one's easy!
        self.assertEqual(self.task.get_dependencies(None), [])

    # TODO: optimize_task


if __name__ == '__main__':
    main()
