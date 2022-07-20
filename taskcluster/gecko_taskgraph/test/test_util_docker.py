# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import shutil
import stat
import tarfile
import tempfile
import unittest
from unittest import mock
import taskcluster_urls as liburls

from gecko_taskgraph.util import docker
from mozunit import main, MockedOpen


MODE_STANDARD = stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH


@mock.patch.dict("os.environ", {"TASKCLUSTER_ROOT_URL": liburls.test_root_url()})
class TestDocker(unittest.TestCase):
    def test_generate_context_hash(self):
        tmpdir = tempfile.mkdtemp()
        try:
            os.makedirs(os.path.join(tmpdir, "docker", "my-image"))
            p = os.path.join(tmpdir, "docker", "my-image", "Dockerfile")
            with open(p, "w") as f:
                f.write("FROM node\nADD a-file\n")
            os.chmod(p, MODE_STANDARD)
            p = os.path.join(tmpdir, "docker", "my-image", "a-file")
            with open(p, "w") as f:
                f.write("data\n")
            os.chmod(p, MODE_STANDARD)
            self.assertIn(
                docker.generate_context_hash(
                    tmpdir,
                    os.path.join(tmpdir, "docker/my-image"),
                    "my-image",
                    {},
                ),
                (
                    "680532a33c845e3b4f8ea8a7bd697da579b647f28c29f7a0a71e51e6cca33983",
                    "cc02f943ae87b283749369fa9c4f6a74639c27a7b9972c99de58e5d9fb3a98ae",
                ),
            )
        finally:
            shutil.rmtree(tmpdir)

    def test_docker_image_explicit_registry(self):
        files = {}
        files[f"{docker.IMAGE_DIR}/myimage/REGISTRY"] = "cool-images"
        files[f"{docker.IMAGE_DIR}/myimage/VERSION"] = "1.2.3"
        files[f"{docker.IMAGE_DIR}/myimage/HASH"] = "sha256:434..."
        with MockedOpen(files):
            self.assertEqual(
                docker.docker_image("myimage"), "cool-images/myimage@sha256:434..."
            )

    def test_docker_image_explicit_registry_by_tag(self):
        files = {}
        files[f"{docker.IMAGE_DIR}/myimage/REGISTRY"] = "myreg"
        files[f"{docker.IMAGE_DIR}/myimage/VERSION"] = "1.2.3"
        files[f"{docker.IMAGE_DIR}/myimage/HASH"] = "sha256:434..."
        with MockedOpen(files):
            self.assertEqual(
                docker.docker_image("myimage", by_tag=True), "myreg/myimage:1.2.3"
            )

    def test_docker_image_default_registry(self):
        files = {}
        files[f"{docker.IMAGE_DIR}/REGISTRY"] = "mozilla"
        files[f"{docker.IMAGE_DIR}/myimage/VERSION"] = "1.2.3"
        files[f"{docker.IMAGE_DIR}/myimage/HASH"] = "sha256:434..."
        with MockedOpen(files):
            self.assertEqual(
                docker.docker_image("myimage"), "mozilla/myimage@sha256:434..."
            )

    def test_docker_image_default_registry_by_tag(self):
        files = {}
        files[f"{docker.IMAGE_DIR}/REGISTRY"] = "mozilla"
        files[f"{docker.IMAGE_DIR}/myimage/VERSION"] = "1.2.3"
        files[f"{docker.IMAGE_DIR}/myimage/HASH"] = "sha256:434..."
        with MockedOpen(files):
            self.assertEqual(
                docker.docker_image("myimage", by_tag=True), "mozilla/myimage:1.2.3"
            )

    def test_create_context_tar_basic(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, "test_image")
            os.mkdir(d)
            with open(os.path.join(d, "Dockerfile"), "a"):
                pass
            os.chmod(os.path.join(d, "Dockerfile"), MODE_STANDARD)

            with open(os.path.join(d, "extra"), "a"):
                pass
            os.chmod(os.path.join(d, "extra"), MODE_STANDARD)

            tp = os.path.join(tmp, "tar")
            h = docker.create_context_tar(tmp, d, tp, "my_image", {})
            self.assertIn(
                h,
                (
                    "eae3ad00936085eb3e5958912f79fb06ee8e14a91f7157c5f38625f7ddacb9c7",
                    "9ff54ee091c4f346e94e809b03efae5aa49a5c1db152f9f633682cfa005f7422",
                ),
            )

            # File prefix should be "my_image"
            with tarfile.open(tp, "r:gz") as tf:
                self.assertEqual(
                    tf.getnames(),
                    [
                        "Dockerfile",
                        "extra",
                    ],
                )
        finally:
            shutil.rmtree(tmp)

    def test_create_context_topsrcdir_files(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, "test-image")
            os.mkdir(d)
            with open(os.path.join(d, "Dockerfile"), "wb") as fh:
                fh.write(b"# %include extra/file0\n")
            os.chmod(os.path.join(d, "Dockerfile"), MODE_STANDARD)

            extra = os.path.join(tmp, "extra")
            os.mkdir(extra)
            with open(os.path.join(extra, "file0"), "a"):
                pass
            os.chmod(os.path.join(extra, "file0"), MODE_STANDARD)

            tp = os.path.join(tmp, "tar")
            h = docker.create_context_tar(tmp, d, tp, "test_image", {})
            self.assertIn(
                h,
                (
                    "49dc3827530cd344d7bcc52e1fdd4aefc632568cf442cffd3dd9633a58f271bf",
                    "8f8e3dd2b712003cd12bb39e5a84fc2a7c06e891cf481613a52bf3db472c4ca9",
                ),
            )

            with tarfile.open(tp, "r:gz") as tf:
                self.assertEqual(
                    tf.getnames(),
                    [
                        "Dockerfile",
                        "topsrcdir/extra/file0",
                    ],
                )
        finally:
            shutil.rmtree(tmp)

    def test_create_context_absolute_path(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, "test-image")
            os.mkdir(d)

            # Absolute paths in %include syntax are not allowed.
            with open(os.path.join(d, "Dockerfile"), "wb") as fh:
                fh.write(b"# %include /etc/shadow\n")

            with self.assertRaisesRegexp(Exception, "cannot be absolute"):
                docker.create_context_tar(tmp, d, os.path.join(tmp, "tar"), "test", {})
        finally:
            shutil.rmtree(tmp)

    def test_create_context_outside_topsrcdir(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, "test-image")
            os.mkdir(d)

            with open(os.path.join(d, "Dockerfile"), "wb") as fh:
                fh.write(b"# %include foo/../../../etc/shadow\n")

            with self.assertRaisesRegexp(Exception, "path outside topsrcdir"):
                docker.create_context_tar(tmp, d, os.path.join(tmp, "tar"), "test", {})
        finally:
            shutil.rmtree(tmp)

    def test_create_context_missing_extra(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, "test-image")
            os.mkdir(d)

            with open(os.path.join(d, "Dockerfile"), "wb") as fh:
                fh.write(b"# %include does/not/exist\n")

            with self.assertRaisesRegexp(Exception, "path does not exist"):
                docker.create_context_tar(tmp, d, os.path.join(tmp, "tar"), "test", {})
        finally:
            shutil.rmtree(tmp)

    def test_create_context_extra_directory(self):
        tmp = tempfile.mkdtemp()
        try:
            d = os.path.join(tmp, "test-image")
            os.mkdir(d)

            with open(os.path.join(d, "Dockerfile"), "wb") as fh:
                fh.write(b"# %include extra\n")
                fh.write(b"# %include file0\n")
            os.chmod(os.path.join(d, "Dockerfile"), MODE_STANDARD)

            extra = os.path.join(tmp, "extra")
            os.mkdir(extra)
            for i in range(3):
                p = os.path.join(extra, "file%d" % i)
                with open(p, "wb") as fh:
                    fh.write(b"file%d" % i)
                os.chmod(p, MODE_STANDARD)

            with open(os.path.join(tmp, "file0"), "a"):
                pass
            os.chmod(os.path.join(tmp, "file0"), MODE_STANDARD)

            tp = os.path.join(tmp, "tar")
            h = docker.create_context_tar(tmp, d, tp, "my_image", {})

            self.assertIn(
                h,
                (
                    "a392f23cd6606ae43116390a4d0113354cff1e688a41d46f48b0fb25e90baa13",
                    "02325bdc508c2e941959170beeb840f6bb91d0675cb8095783a7db7301d136b2",
                ),
            )

            with tarfile.open(tp, "r:gz") as tf:
                self.assertEqual(
                    tf.getnames(),
                    [
                        "Dockerfile",
                        "topsrcdir/extra/file0",
                        "topsrcdir/extra/file1",
                        "topsrcdir/extra/file2",
                        "topsrcdir/file0",
                    ],
                )
        finally:
            shutil.rmtree(tmp)


if __name__ == "__main__":
    main()
