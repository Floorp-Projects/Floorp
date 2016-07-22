# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import shutil
import stat
import tarfile
import tempfile
import unittest

from ..util import docker
from mozunit import MockedOpen


MODE_STANDARD = stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH


class TestDocker(unittest.TestCase):

    def test_generate_context_hash(self):
        tmpdir = tempfile.mkdtemp()
        old_GECKO = docker.GECKO
        docker.GECKO = tmpdir
        try:
            os.makedirs(os.path.join(tmpdir, 'docker', 'my-image'))
            with open(os.path.join(tmpdir, 'docker', 'my-image', 'Dockerfile'), "w") as f:
                f.write("FROM node\nADD a-file\n")
            with open(os.path.join(tmpdir, 'docker', 'my-image', 'a-file'), "w") as f:
                f.write("data\n")
            self.assertEqual(
                docker.generate_context_hash('docker/my-image'),
                '781143fcc6cc72c9024b058665265cb6bae3fb8031cad7227dd169ffbfced434'
                )
        finally:
            docker.GECKO = old_GECKO
            shutil.rmtree(tmpdir)

    def test_docker_image_explicit_registry(self):
        files = {}
        files["{}/myimage/REGISTRY".format(docker.DOCKER_ROOT)] = "cool-images"
        files["{}/myimage/VERSION".format(docker.DOCKER_ROOT)] = "1.2.3"
        with MockedOpen(files):
            self.assertEqual(docker.docker_image('myimage'), "cool-images/myimage:1.2.3")

    def test_docker_image_default_registry(self):
        files = {}
        files["{}/REGISTRY".format(docker.DOCKER_ROOT)] = "mozilla"
        files["{}/myimage/VERSION".format(docker.DOCKER_ROOT)] = "1.2.3"
        with MockedOpen(files):
            self.assertEqual(docker.docker_image('myimage'), "mozilla/myimage:1.2.3")

    def test_create_context_tar_basic(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, 'test_image')
            os.mkdir(d)
            with open(os.path.join(d, 'Dockerfile'), 'a'):
                pass
            os.chmod(os.path.join(d, 'Dockerfile'), MODE_STANDARD)

            with open(os.path.join(d, 'extra'), 'a'):
                pass
            os.chmod(os.path.join(d, 'extra'), MODE_STANDARD)

            tp = os.path.join(tmp, 'tar')
            h = docker.create_context_tar(tmp, d, tp, 'my_image')
            self.assertEqual(h, '2a6d7f1627eba60daf85402418e041d728827d309143c6bc1c6bb3035bde6717')

            # File prefix should be "my_image"
            with tarfile.open(tp, 'r:gz') as tf:
                self.assertEqual(tf.getnames(), [
                    'my_image/Dockerfile',
                    'my_image/extra',
                ])
        finally:
            shutil.rmtree(tmp)
