# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import yaml

from taskgraph.util.memoize import memoize


EXTENSIONS = {
    'aar': ('.aar', '.pom', '-sources.jar'),
    'jar': ('.jar', '.pom', '-sources.jar')
}
CHECKSUMS_EXTENSIONS = ('.sha1', '.md5')
CURRENT_DIR = os.path.dirname(os.path.realpath(__file__))
PROJECT_DIR = os.path.realpath(os.path.join(CURRENT_DIR, '..', '..'))


def get_components():
    build_config = _read_build_config()
    return [{
        'name': name,
        'path': project['path'],
        'shouldPublish': project['publish']
    } for (name, project) in build_config['projects'].items()]


def get_version():
    with open(os.path.join(PROJECT_DIR, "version.txt")) as fh:
        return fh.read().strip()


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
    with open(os.path.join(PROJECT_DIR, '.buildconfig.yml'), 'rb') as f:
        return yaml.safe_load(f)
