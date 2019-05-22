# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
import os
import yaml

cached_build_config = None


def read_build_config():
    global cached_build_config

    if cached_build_config is None:
        with open(os.path.join(os.path.dirname(__file__), '..', '..', '..', '.buildconfig.yml'), 'rb') as f:
            cached_build_config = yaml.safe_load(f)
    return cached_build_config


def components():
    build_config = read_build_config()
    return [{
        'name': name,
        'path': project['path'],
        'shouldPublish': project['publish']
    } for (name, project) in build_config['projects'].items()]


def components_version():
    build_config = read_build_config()
    return build_config['componentsVersion']
