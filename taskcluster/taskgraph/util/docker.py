# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import hashlib
import os

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
