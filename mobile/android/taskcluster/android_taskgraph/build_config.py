# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import yaml

from taskgraph.util.memoize import memoize

from android_taskgraph import PROJECT_DIR, ANDROID_COMPONENTS_DIR, FOCUS_DIR


EXTENSIONS = {
    'aar': ('.aar', '.pom', '-sources.jar'),
    'jar': ('.jar', '.pom', '-sources.jar')
}
CHECKSUMS_EXTENSIONS = ('.sha1', '.md5')


def get_components():
    build_config = _read_build_config(ANDROID_COMPONENTS_DIR)
    return [{
        'name': name,
        'path': project['path'],
        'shouldPublish': project['publish']
    } for (name, project) in build_config['projects'].items()]


def get_version():
    with open(os.path.join(PROJECT_DIR, "version.txt")) as fh:
        return fh.read().strip()


def get_path(component):
    return _read_build_config(ANDROID_COMPONENTS_DIR)["projects"][component]["path"]


def get_extensions(component):
    artifact_type = _read_build_config(ANDROID_COMPONENTS_DIR)["projects"][component].get("artifact-type", "aar")
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
def _read_build_config(root_dir):
    with open(os.path.join(root_dir, '.buildconfig.yml'), 'rb') as f:
        return yaml.safe_load(f)


@memoize
def get_upstream_deps_for_all_gradle_projects():
    all_deps = {}
    for root_dir in (ANDROID_COMPONENTS_DIR, FOCUS_DIR):
        build_config = _read_build_config(root_dir)
        all_deps.update({
            project: project_config["upstream_dependencies"]
            for project, project_config in build_config["projects"].items()
        })

    return all_deps


def get_apk_based_projects():
    # TODO: Support Fenix
    return [{
        "name": "focus",
        "path": FOCUS_DIR,
    }]


def get_variant(build_type, build_name):
    # TODO: Support Fenix
    all_variants = _read_build_config(FOCUS_DIR)["variants"]
    matching_variants = [
        variant for variant in all_variants
        if variant["build_type"] == build_type and variant["name"] == build_name
    ]
    number_of_matching_variants = len(matching_variants)
    if number_of_matching_variants == 0:
        raise ValueError('No variant found for build type "{}"'.format(
            build_type
        ))
    elif number_of_matching_variants > 1:
        raise ValueError('Too many variants found for build type "{}"": {}'.format(
            build_type, matching_variants
        ))

    return matching_variants.pop()
