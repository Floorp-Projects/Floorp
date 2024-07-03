# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
from functools import lru_cache

import yaml

from android_taskgraph import ANDROID_COMPONENTS_DIR, FENIX_DIR, FOCUS_DIR

EXTENSIONS = {
    "aar": (".aar", ".pom", "-sources.jar"),
    "jar": (".jar", ".pom", "-sources.jar"),
}
CHECKSUMS_EXTENSIONS = (".md5", ".sha1", ".sha256", ".sha512")


def get_components():
    build_config = _read_build_config(ANDROID_COMPONENTS_DIR)
    return [
        {"name": name, "path": project["path"], "shouldPublish": project["publish"]}
        for (name, project) in build_config["projects"].items()
    ]


def get_path(component):
    return _read_build_config(ANDROID_COMPONENTS_DIR)["projects"][component]["path"]


def get_extensions(component):
    artifact_type = _read_build_config(ANDROID_COMPONENTS_DIR)["projects"][
        component
    ].get("artifact-type", "aar")
    if artifact_type not in EXTENSIONS:
        raise ValueError(
            "For '{}', 'artifact-type' must be one of {}".format(
                component, repr(EXTENSIONS.keys())
            )
        )

    return [
        extension + checksum_extension
        for extension in EXTENSIONS[artifact_type]
        for checksum_extension in ("",) + CHECKSUMS_EXTENSIONS
    ]


@lru_cache(maxsize=None)
def _read_build_config(root_dir):
    with open(os.path.join(root_dir, ".buildconfig.yml"), "rb") as f:
        return yaml.safe_load(f)


def get_apk_based_projects():
    return [
        {
            "name": "focus",
            "path": FOCUS_DIR,
        },
        {
            "name": "fenix",
            "path": FENIX_DIR,
        },
    ]


def get_variant(build_type, build_name):
    all_variants = _get_all_variants()
    matching_variants = [
        variant
        for variant in all_variants
        if variant["build_type"] == build_type and variant["name"] == build_name
    ]
    number_of_matching_variants = len(matching_variants)
    if number_of_matching_variants == 0:
        raise ValueError('No variant found for build type "{}"'.format(build_type))
    elif number_of_matching_variants > 1:
        raise ValueError(
            'Too many variants found for build type "{}"": {}'.format(
                build_type, matching_variants
            )
        )

    return matching_variants.pop()


def _get_all_variants():
    all_variants_including_duplicates = (
        _read_build_config(FOCUS_DIR)["variants"]
        + _read_build_config(FENIX_DIR)["variants"]
    )
    all_unique_variants = []
    for variant in all_variants_including_duplicates:
        if (
            # androidTest is a special case that can't be prefixed with fenix or focus.
            # Hence, this variant exist in both build_config and we need to expose it
            # once only.
            (
                variant["build_type"] != "androidTest"
                and variant["name"] != "androidTest"
            )
            or variant not in all_unique_variants
        ):
            all_unique_variants.append(variant)

    return all_unique_variants
