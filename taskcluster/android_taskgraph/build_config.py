# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os

import yaml
from taskgraph.util.memoize import memoize

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


@memoize
def _read_build_config(root_dir):
    with open(os.path.join(root_dir, ".buildconfig.yml"), "rb") as f:
        return yaml.safe_load(f)


@memoize
def get_upstream_deps_for_all_gradle_projects():
    all_deps = {}
    for root_dir in (ANDROID_COMPONENTS_DIR, FOCUS_DIR, FENIX_DIR):
        build_config = _read_build_config(root_dir)
        new_deps = {
            project: project_config["upstream_dependencies"]
            for project, project_config in build_config["projects"].items()
        }

        app_config = new_deps.pop("app", None)
        if app_config:
            if root_dir == FOCUS_DIR:
                gradle_project = "focus"
            elif root_dir == FENIX_DIR:
                gradle_project = "fenix"
            else:
                raise NotImplementedError(f"Unsupported root_dir {root_dir}")
            new_deps[gradle_project] = app_config

        all_deps.update(new_deps)

    return all_deps


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
