# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import hashlib
import os

from mozpack.archive import (
    create_tar_gz_from_files,
)


GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..', '..'))
DOCKER_ROOT = os.path.join(GECKO, 'testing', 'docker')
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'


def docker_image(name):
    '''Determine the docker image name, including repository and tag, from an
    in-tree docker file.'''
    try:
        with open(os.path.join(DOCKER_ROOT, name, 'REGISTRY')) as f:
            registry = f.read().strip()
    except IOError:
        with open(os.path.join(DOCKER_ROOT, 'REGISTRY')) as f:
            registry = f.read().strip()

    with open(os.path.join(DOCKER_ROOT, name, 'VERSION')) as f:
        version = f.read().strip()

    return '{}/{}:{}'.format(registry, name, version)


def generate_context_hash(image_path):
    '''Generates a sha256 hash for context directory used to build an image.

    Contents of the directory are sorted alphabetically, contents of each file is hashed,
    and then a hash is created for both the file hashs as well as their paths.

    This ensures that hashs are consistent and also change based on if file locations
    within the context directory change.
    '''
    context_hash = hashlib.sha256()
    files = []

    for dirpath, dirnames, filenames in os.walk(os.path.join(GECKO, image_path)):
        for filename in filenames:
            files.append(os.path.join(dirpath, filename))

    for filename in sorted(files):
        relative_filename = filename.replace(GECKO, '')
        with open(filename, 'rb') as f:
            file_hash = hashlib.sha256()
            data = f.read()
            file_hash.update(data)
            context_hash.update(file_hash.hexdigest() + '\t' + relative_filename + '\n')

    return context_hash.hexdigest()


def create_context_tar(context_dir, out_path, prefix):
    """Create a context tarball.

    A directory ``context_dir`` containing a Dockerfile will be assembled into
    a gzipped tar file at ``out_path``. Files inside the archive will be
    prefixed by directory ``prefix``.

    Returns the SHA-256 hex digest of the created archive.
    """
    archive_files = {}

    for root, dirs, files in os.walk(context_dir):
        for f in files:
            source_path = os.path.join(root, f)
            rel = source_path[len(context_dir) + 1:]
            archive_path = os.path.join(prefix, rel)
            archive_files[archive_path] = source_path

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
