# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import hashlib
import os
import shutil
import subprocess
import tarfile
import tempfile

from mozpack.archive import (
    create_tar_gz_from_files,
)


GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..', '..'))
DOCKER_ROOT = os.path.join(GECKO, 'testing', 'docker')
INDEX_PREFIX = 'docker.images.v2'
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'


def docker_image(name, default_version=None):
    '''Determine the docker image name, including repository and tag, from an
    in-tree docker file.'''
    try:
        with open(os.path.join(DOCKER_ROOT, name, 'REGISTRY')) as f:
            registry = f.read().strip()
    except IOError:
        with open(os.path.join(DOCKER_ROOT, 'REGISTRY')) as f:
            registry = f.read().strip()

    try:
        with open(os.path.join(DOCKER_ROOT, name, 'VERSION')) as f:
            version = f.read().strip()
    except IOError:
        if not default_version:
            raise

        version = default_version

    return '{}/{}:{}'.format(registry, name, version)


def generate_context_hash(topsrcdir, image_path, image_name):
    """Generates a sha256 hash for context directory used to build an image."""

    # It is a bit unfortunate we have to create a temp file here - it would
    # be nicer to use an in-memory buffer.
    fd, p = tempfile.mkstemp()
    os.close(fd)
    try:
        return create_context_tar(topsrcdir, image_path, p, image_name)
    finally:
        os.unlink(p)


def create_context_tar(topsrcdir, context_dir, out_path, prefix):
    """Create a context tarball.

    A directory ``context_dir`` containing a Dockerfile will be assembled into
    a gzipped tar file at ``out_path``. Files inside the archive will be
    prefixed by directory ``prefix``.

    We also scan the source Dockerfile for special syntax that influences
    context generation.

    If a line in the Dockerfile has the form ``# %include <path>``,
    the relative path specified on that line will be matched against
    files in the source repository and added to the context under the
    path ``topsrcdir/``. If an entry is a directory, we add all files
    under that directory.

    Returns the SHA-256 hex digest of the created archive.
    """
    archive_files = {}

    for root, dirs, files in os.walk(context_dir):
        for f in files:
            source_path = os.path.join(root, f)
            rel = source_path[len(context_dir) + 1:]
            archive_path = os.path.join(prefix, rel)
            archive_files[archive_path] = source_path

    # Parse Dockerfile for special syntax of extra files to include.
    with open(os.path.join(context_dir, 'Dockerfile'), 'rb') as fh:
        for line in fh:
            line = line.rstrip()
            if not line.startswith('# %include'):
                continue

            p = line[len('# %include '):].strip()
            if os.path.isabs(p):
                raise Exception('extra include path cannot be absolute: %s' % p)

            fs_path = os.path.normpath(os.path.join(topsrcdir, p))
            # Check for filesystem traversal exploits.
            if not fs_path.startswith(topsrcdir):
                raise Exception('extra include path outside topsrcdir: %s' % p)

            if not os.path.exists(fs_path):
                raise Exception('extra include path does not exist: %s' % p)

            if os.path.isdir(fs_path):
                for root, dirs, files in os.walk(fs_path):
                    for f in files:
                        source_path = os.path.join(root, f)
                        archive_path = os.path.join(prefix, 'topsrcdir', p, f)
                        archive_files[archive_path] = source_path
            else:
                archive_path = os.path.join(prefix, 'topsrcdir', p)
                archive_files[archive_path] = fs_path

    with open(out_path, 'wb') as fh:
        create_tar_gz_from_files(fh, archive_files, '%s.tar.gz' % prefix)

    h = hashlib.sha256()
    with open(out_path, 'rb') as fh:
        while True:
            data = fh.read(32768)
            if not data:
                break
            h.update(data)
    return h.hexdigest()


def build_from_context(docker_bin, context_path, prefix, tag=None):
    """Build a Docker image from a context archive.

    Given the path to a `docker` binary, a image build tar.gz (produced with
    ``create_context_tar()``, a prefix in that context containing files, and
    an optional ``tag`` for the produced image, build that Docker image.
    """
    d = tempfile.mkdtemp()
    try:
        with tarfile.open(context_path, 'r:gz') as tf:
            tf.extractall(d)

        # If we wanted to do post-processing of the Dockerfile, this is
        # where we'd do it.

        args = [
            docker_bin,
            'build',
            # Use --no-cache so we always get the latest package updates.
            '--no-cache',
        ]

        if tag:
            args.extend(['-t', tag])

        args.append('.')

        res = subprocess.call(args, cwd=os.path.join(d, prefix))
        if res:
            raise Exception('error building image')
    finally:
        shutil.rmtree(d)
