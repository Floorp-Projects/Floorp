# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
import tempfile
import shutil
import os

from ..kind import docker_image
from ..types import Task
from mozunit import main, MockedOpen


class TestDockerImageKind(unittest.TestCase):

    def setUp(self):
        self.kind = docker_image.DockerImageKind(
                os.path.join(docker_image.GECKO, 'taskcluster', 'ci', 'docker-image'),
                {})

    def test_get_task_dependencies(self):
        # this one's easy!
        self.assertEqual(self.kind.get_task_dependencies(None, None), [])

    # TODO: optimize_task

    def test_create_context_tar(self):
        image_dir = os.path.join(docker_image.GECKO, 'testing', 'docker', 'image_builder')
        tarball = tempfile.mkstemp()[1]
        self.kind.create_context_tar(image_dir, tarball, 'image_builder')
        self.failUnless(os.path.exists(tarball))
        os.unlink(tarball)

    def test_generate_context_hash(self):
        tmpdir = tempfile.mkdtemp()
        old_GECKO = docker_image.GECKO
        docker_image.GECKO = tmpdir
        try:
            os.makedirs(os.path.join(tmpdir, 'docker', 'my-image'))
            with open(os.path.join(tmpdir, 'docker', 'my-image', 'Dockerfile'), "w") as f:
                f.write("FROM node\nADD a-file\n")
            with open(os.path.join(tmpdir, 'docker', 'my-image', 'a-file'), "w") as f:
                f.write("data\n")
            self.assertEqual(self.kind.generate_context_hash('docker/my-image'),
                    '781143fcc6cc72c9024b058665265cb6bae3fb8031cad7227dd169ffbfced434')
        finally:
            docker_image.GECKO = old_GECKO
            shutil.rmtree(tmpdir)

if __name__ == '__main__':
    main()
