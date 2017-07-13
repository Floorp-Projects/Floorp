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

from taskgraph.util import docker
from mozunit import main, MockedOpen


MODE_STANDARD = stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH


class TestDocker(unittest.TestCase):

    def test_generate_context_hash(self):
        tmpdir = tempfile.mkdtemp()
        old_GECKO = docker.GECKO
        docker.GECKO = tmpdir
        try:
            os.makedirs(os.path.join(tmpdir, 'docker', 'my-image'))
            p = os.path.join(tmpdir, 'docker', 'my-image', 'Dockerfile')
            with open(p, 'w') as f:
                f.write("FROM node\nADD a-file\n")
            os.chmod(p, MODE_STANDARD)
            p = os.path.join(tmpdir, 'docker', 'my-image', 'a-file')
            with open(p, 'w') as f:
                f.write("data\n")
            os.chmod(p, MODE_STANDARD)
            self.assertEqual(
                docker.generate_context_hash(docker.GECKO,
                                             os.path.join(docker.GECKO, 'docker/my-image'),
                                             'my-image'),
                'e61e675ce05e8c11424437db3f1004079374c1a5fe6ad6800346cebe137b0797'
            )
        finally:
            docker.GECKO = old_GECKO
            shutil.rmtree(tmpdir)

    def test_docker_image_explicit_registry(self):
        files = {}
        files["{}/myimage/REGISTRY".format(docker.IMAGE_DIR)] = "cool-images"
        files["{}/myimage/VERSION".format(docker.IMAGE_DIR)] = "1.2.3"
        files["{}/myimage/HASH".format(docker.IMAGE_DIR)] = "sha256:434..."
        with MockedOpen(files):
            self.assertEqual(docker.docker_image('myimage'), "cool-images/myimage@sha256:434...")

    def test_docker_image_explicit_registry_by_tag(self):
        files = {}
        files["{}/myimage/REGISTRY".format(docker.IMAGE_DIR)] = "myreg"
        files["{}/myimage/VERSION".format(docker.IMAGE_DIR)] = "1.2.3"
        files["{}/myimage/HASH".format(docker.IMAGE_DIR)] = "sha256:434..."
        with MockedOpen(files):
            self.assertEqual(docker.docker_image('myimage', by_tag=True), "myreg/myimage:1.2.3")

    def test_docker_image_default_registry(self):
        files = {}
        files["{}/REGISTRY".format(docker.IMAGE_DIR)] = "mozilla"
        files["{}/myimage/VERSION".format(docker.IMAGE_DIR)] = "1.2.3"
        files["{}/myimage/HASH".format(docker.IMAGE_DIR)] = "sha256:434..."
        with MockedOpen(files):
            self.assertEqual(docker.docker_image('myimage'), "mozilla/myimage@sha256:434...")

    def test_docker_image_default_registry_by_tag(self):
        files = {}
        files["{}/REGISTRY".format(docker.IMAGE_DIR)] = "mozilla"
        files["{}/myimage/VERSION".format(docker.IMAGE_DIR)] = "1.2.3"
        files["{}/myimage/HASH".format(docker.IMAGE_DIR)] = "sha256:434..."
        with MockedOpen(files):
            self.assertEqual(docker.docker_image('myimage', by_tag=True), "mozilla/myimage:1.2.3")

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

    def test_create_context_topsrcdir_files(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, 'test-image')
            os.mkdir(d)
            with open(os.path.join(d, 'Dockerfile'), 'wb') as fh:
                fh.write(b'# %include extra/file0\n')
            os.chmod(os.path.join(d, 'Dockerfile'), MODE_STANDARD)

            extra = os.path.join(tmp, 'extra')
            os.mkdir(extra)
            with open(os.path.join(extra, 'file0'), 'a'):
                pass
            os.chmod(os.path.join(extra, 'file0'), MODE_STANDARD)

            tp = os.path.join(tmp, 'tar')
            h = docker.create_context_tar(tmp, d, tp, 'test_image')
            self.assertEqual(h, '20faeb7c134f21187b142b5fadba94ae58865dc929c6c293d8cbc0a087269338')

            with tarfile.open(tp, 'r:gz') as tf:
                self.assertEqual(tf.getnames(), [
                    'test_image/Dockerfile',
                    'test_image/topsrcdir/extra/file0',
                ])
        finally:
            shutil.rmtree(tmp)

    def test_create_context_absolute_path(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, 'test-image')
            os.mkdir(d)

            # Absolute paths in %include syntax are not allowed.
            with open(os.path.join(d, 'Dockerfile'), 'wb') as fh:
                fh.write(b'# %include /etc/shadow\n')

            with self.assertRaisesRegexp(Exception, 'cannot be absolute'):
                docker.create_context_tar(tmp, d, os.path.join(tmp, 'tar'), 'test')
        finally:
            shutil.rmtree(tmp)

    def test_create_context_outside_topsrcdir(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, 'test-image')
            os.mkdir(d)

            with open(os.path.join(d, 'Dockerfile'), 'wb') as fh:
                fh.write(b'# %include foo/../../../etc/shadow\n')

            with self.assertRaisesRegexp(Exception, 'path outside topsrcdir'):
                docker.create_context_tar(tmp, d, os.path.join(tmp, 'tar'), 'test')
        finally:
            shutil.rmtree(tmp)

    def test_create_context_missing_extra(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, 'test-image')
            os.mkdir(d)

            with open(os.path.join(d, 'Dockerfile'), 'wb') as fh:
                fh.write(b'# %include does/not/exist\n')

            with self.assertRaisesRegexp(Exception, 'path does not exist'):
                docker.create_context_tar(tmp, d, os.path.join(tmp, 'tar'), 'test')
        finally:
            shutil.rmtree(tmp)

    def test_create_context_extra_directory(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, 'test-image')
            os.mkdir(d)

            with open(os.path.join(d, 'Dockerfile'), 'wb') as fh:
                fh.write(b'# %include extra\n')
                fh.write(b'# %include file0\n')
            os.chmod(os.path.join(d, 'Dockerfile'), MODE_STANDARD)

            extra = os.path.join(tmp, 'extra')
            os.mkdir(extra)
            for i in range(3):
                p = os.path.join(extra, 'file%d' % i)
                with open(p, 'wb') as fh:
                    fh.write(b'file%d' % i)
                os.chmod(p, MODE_STANDARD)

            with open(os.path.join(tmp, 'file0'), 'a'):
                pass
            os.chmod(os.path.join(tmp, 'file0'), MODE_STANDARD)

            tp = os.path.join(tmp, 'tar')
            h = docker.create_context_tar(tmp, d, tp, 'my_image')

            self.assertEqual(h, 'e5440513ab46ae4c1d056269e1c6715d5da7d4bd673719d360411e35e5b87205')

            with tarfile.open(tp, 'r:gz') as tf:
                self.assertEqual(tf.getnames(), [
                    'my_image/Dockerfile',
                    'my_image/topsrcdir/extra/file0',
                    'my_image/topsrcdir/extra/file1',
                    'my_image/topsrcdir/extra/file2',
                    'my_image/topsrcdir/file0',
                ])
        finally:
            shutil.rmtree(tmp)


if __name__ == '__main__':
    main()
