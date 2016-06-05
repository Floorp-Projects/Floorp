# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..util.docker import docker_image, DOCKER_ROOT
from mozunit import main, MockedOpen


class TestDockerImage(unittest.TestCase):

    def test_docker_image_explicit_registry(self):
        files = {}
        files["{}/myimage/REGISTRY".format(DOCKER_ROOT)] = "cool-images"
        files["{}/myimage/VERSION".format(DOCKER_ROOT)] = "1.2.3"
        with MockedOpen(files):
            self.assertEqual(docker_image('myimage'), "cool-images/myimage:1.2.3")

    def test_docker_image_default_registry(self):
        files = {}
        files["{}/REGISTRY".format(DOCKER_ROOT)] = "mozilla"
        files["{}/myimage/VERSION".format(DOCKER_ROOT)] = "1.2.3"
        with MockedOpen(files):
            self.assertEqual(docker_image('myimage'), "mozilla/myimage:1.2.3")
