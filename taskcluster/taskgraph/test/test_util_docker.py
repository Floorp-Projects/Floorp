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
import mock
import taskcluster_urls as liburls

from taskgraph.util import docker
from mozunit import main, MockedOpen


MODE_STANDARD = stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH


@mock.patch.dict('os.environ', {'TASKCLUSTER_ROOT_URL': liburls.test_root_url()})
class TestDocker(unittest.TestCase):
    def test_generate_context_hash(self):
        tmpdir = tempfile.mkdtemp()
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
                docker.generate_context_hash(tmpdir,
                                             os.path.join(tmpdir, 'docker/my-image'),
                                             'my-image'),
                'e1649b3427bd7a0387f4508d25057c2e89228748517aad6c70e3df54f47bd13a'
            )
        finally:
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
            self.assertEqual(h, '6c1cc23357625f64f775a08eace7bbc3877dd08d2f3546e0f2e308bac8491865')

            # File prefix should be "my_image"
            with tarfile.open(tp, 'r:gz') as tf:
                self.assertEqual(tf.getnames(), [
                    'Dockerfile',
                    'extra',
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
            self.assertEqual(h, 'e7f14044b8ec1ba42e251d4b293af212ad08b30ec8ab6613abbdbe73c3c2b61f')

            with tarfile.open(tp, 'r:gz') as tf:
                self.assertEqual(tf.getnames(), [
                    'Dockerfile',
                    'topsrcdir/extra/file0',
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

            self.assertEqual(h, 'd2a3363b15d0eb547a6c81a72ddf3980e2f6e6360c29b4fb6818102896f43180')

            with tarfile.open(tp, 'r:gz') as tf:
                self.assertEqual(tf.getnames(), [
                    'Dockerfile',
                    'topsrcdir/extra/file0',
                    'topsrcdir/extra/file1',
                    'topsrcdir/extra/file2',
                    'topsrcdir/file0',
                ])
        finally:
            shutil.rmtree(tmp)


if __name__ == '__main__':
    main()
