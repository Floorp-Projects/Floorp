# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..'))
DOCKER_ROOT = os.path.join(GECKO, 'testing', 'docker')

def docker_image(name):
    ''' Determine the docker image name, including repository and tag, from an
    in-tree docker file'''
    try:
        with open(os.path.join(DOCKER_ROOT, name, 'REGISTRY')) as f:
            registry = f.read().strip()
    except IOError:
        with open(os.path.join(DOCKER_ROOT, 'REGISTRY')) as f:
            registry = f.read().strip()

    with open(os.path.join(DOCKER_ROOT, name, 'VERSION')) as f:
        version = f.read().strip()

    return '{}/{}:{}'.format(registry, name, version)
