# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import yaml

from six import ensure_text

from taskgraph.util.memoize import memoize


EXTENSIONS = {
    'aar': ('.aar', '.pom', '-sources.jar'),
    'jar': ('.jar', '.pom', '-sources.jar')
}
CHECKSUMS_EXTENSIONS = ('.sha1', '.md5')


def get_components():
    build_config = _read_build_config()
    return [{
        'name': name,
        'path': project['path'],
        'shouldPublish': project['publish']
    } for (name, project) in build_config['projects'].items()]


def get_version():
    return ensure_text(_read_build_config()["componentsVersion"])


def get_path(component):
    return _read_build_config()["projects"][component]["path"]


def get_extensions(component):
    artifact_type = _read_build_config()["projects"][component].get("artifact-type", "aar")
    if artifact_type not in EXTENSIONS:
        raise ValueError(
            "For '{}', 'artifact-type' must be one of {}".format(
                component, repr(EXTENSIONS.keys())
            )
        )

    return [
        extension + checksum_extension
        for extension in EXTENSIONS[artifact_type]
        for checksum_extension in ('',) + CHECKSUMS_EXTENSIONS
    ]


@memoize
def _read_build_config():
    current_dir = os.path.dirname(os.path.realpath(__file__))
    project_dir = os.path.realpath(os.path.join(current_dir, '..', '..'))

    with open(os.path.join(project_dir, '.buildconfig.yml'), 'rb') as f:
        return yaml.safe_load(f)
