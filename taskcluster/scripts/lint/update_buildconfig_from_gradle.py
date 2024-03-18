#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import argparse
import json
import logging
import os
import re
import subprocess
import sys
from collections import defaultdict

import yaml
from mergedeep import merge

logger = logging.getLogger(__name__)

_DEFAULT_GRADLE_COMMAND = ("./gradlew", "--console=plain", "--no-parallel")
_LOCAL_DEPENDENCY_PATTERN = re.compile(
    r"(\+|\\)--- project :(?P<local_dependency_name>\S+)\s?.*"
)


def _get_upstream_deps_per_gradle_project(gradle_root, existing_build_config):
    project_dependencies = defaultdict(set)
    gradle_projects = _get_gradle_projects(gradle_root, existing_build_config)

    logger.info(f"Looking for dependencies in {gradle_root}")

    # This is eventually going to fail if there's ever enough projects to make the
    # command line too long. If that happens, we'll need to split this list up and
    # run gradle more than once.
    cmd = list(_DEFAULT_GRADLE_COMMAND)
    cmd.extend(
        [f"{gradle_project}:dependencies" for gradle_project in sorted(gradle_projects)]
    )

    # Parsing output like this is not ideal but bhearsum couldn't find a way
    # to get the dependencies printed in a better format. If we could convince
    # gradle to spit out JSON that would be much better.
    # This is filed as https://bugzilla.mozilla.org/show_bug.cgi?id=1795152
    current_project_name = None
    print(f"Running command: {' '.join(cmd)}")
    try:
        output = subprocess.check_output(cmd, universal_newlines=True, cwd=gradle_root)
    except subprocess.CalledProcessError as cpe:
        print(cpe.output)
        raise
    for line in output.splitlines():
        # If we find the start of a new component section, update our tracking
        # variable
        if line.startswith("Project"):
            current_project_name = line.split(":")[1].strip("'")

        # If we find a new local dependency, add it.
        local_dep_match = _LOCAL_DEPENDENCY_PATTERN.search(line)
        if local_dep_match:
            local_dependency_name = local_dep_match.group("local_dependency_name")
            if (
                local_dependency_name != current_project_name
                # These lint rules are not part of android-components
                and local_dependency_name != "mozilla-lint-rules"
            ):
                project_dependencies[current_project_name].add(local_dependency_name)

    return {
        project_name: sorted(project_dependencies[project_name])
        for project_name in gradle_projects
    }


def _get_gradle_projects(gradle_root, existing_build_config):
    if gradle_root.endswith("android-components"):
        return list(existing_build_config["projects"].keys())
    elif gradle_root.endswith("focus-android"):
        return ["app"]
    elif gradle_root.endswith("fenix"):
        return ["app"]

    raise NotImplementedError(f"Cannot find gradle projects for {gradle_root}")


def is_dir(string):
    if os.path.isdir(string):
        return string
    else:
        raise argparse.ArgumentTypeError(f'"{string}" is not a directory')


def _parse_args(cmdln_args):
    parser = argparse.ArgumentParser(
        description="Calls gradle and generate json file with dependencies"
    )
    parser.add_argument(
        "gradle_root",
        metavar="GRADLE_ROOT",
        type=is_dir,
        help="The directory where to call gradle from",
    )
    return parser.parse_args(args=cmdln_args)


def _set_logging_config():
    logging.basicConfig(
        level=logging.DEBUG, format="%(asctime)s - %(levelname)s - %(message)s"
    )


def _merge_build_config(
    existing_build_config, upstream_deps_per_project, variants_config
):
    updated_build_config = {
        "projects": {
            project: {"upstream_dependencies": deps}
            for project, deps in upstream_deps_per_project.items()
        }
    }
    updated_variant_config = {"variants": variants_config} if variants_config else {}
    return merge(existing_build_config, updated_build_config, updated_variant_config)


def _get_variants(gradle_root):
    cmd = list(_DEFAULT_GRADLE_COMMAND) + ["printVariants"]
    output_lines = subprocess.check_output(
        cmd, universal_newlines=True, cwd=gradle_root
    ).splitlines()
    variants_line = [line for line in output_lines if line.startswith("variants: ")][0]
    variants_json = variants_line.split(" ", 1)[1]
    return json.loads(variants_json)


def _should_print_variants(gradle_root):
    return gradle_root.endswith("fenix") or gradle_root.endswith("focus-android")


def main():
    args = _parse_args(sys.argv[1:])
    gradle_root = args.gradle_root
    build_config_file = os.path.join(gradle_root, ".buildconfig.yml")
    _set_logging_config()

    with open(build_config_file) as f:
        existing_build_config = yaml.safe_load(f)

    upstream_deps_per_project = _get_upstream_deps_per_gradle_project(
        gradle_root, existing_build_config
    )

    variants_config = (
        _get_variants(gradle_root) if _should_print_variants(gradle_root) else {}
    )
    merged_build_config = _merge_build_config(
        existing_build_config, upstream_deps_per_project, variants_config
    )

    with open(build_config_file, "w") as f:
        yaml.safe_dump(merged_build_config, f)
    logger.info(f"Updated {build_config_file} with latest gradle config!")


__name__ == "__main__" and main()
