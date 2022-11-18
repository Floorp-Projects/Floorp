# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import logging
import subprocess

from collections import defaultdict
from taskgraph.util.memoize import memoize

from android_taskgraph import FOCUS_DIR
from android_taskgraph.build_config import get_components


logger = logging.getLogger(__name__)

CONFIGURATIONS_WITH_DEPENDENCIES = (
    "api",
    "compileOnly",
    "implementation",
    "testImplementation"
)


@memoize
def get_upstream_deps_for_gradle_projects(gradle_root):
    """Return the full list of local upstream dependencies of a component."""
    project_dependencies = defaultdict(set)
    gradle_projects = _get_gradle_projects(gradle_root)

    for configuration in CONFIGURATIONS_WITH_DEPENDENCIES:
        logger.info(f"Looking for dependencies in {configuration} configuration in {gradle_root}")

        cmd = ["./gradlew", "--console=plain", "--parallel"]
        # This is eventually going to fail if there's ever enough projects to make the command line
        # too long. If that happens, we'll need to split this list up and run gradle more than once.
        for gradle_project in sorted(gradle_projects):
            cmd.extend([f"{gradle_project}:dependencies", "--configuration", configuration])

        # Parsing output like this is not ideal but bhearsum couldn't find a way
        # to get the dependencies printed in a better format. If we could convince
        # gradle to spit out JSON that would be much better.
        # This is filed as https://bugzilla.mozilla.org/show_bug.cgi?id=1795152
        current_project_name = None
        for line in subprocess.check_output(cmd, universal_newlines=True, cwd=gradle_root).splitlines():
            # If we find the start of a new component section, update our tracking variable
            if line.startswith("Project"):
                current_project_name = line.split(":")[1].strip("'")

            # If we find a new local dependency, add it.
            if line.startswith("+--- project") or line.startswith(r"\--- project"):
                project_dependencies[current_project_name].add(line.split(" ")[2])

    return {
        project_name: sorted(project_dependencies[project_name]) for project_name in gradle_projects
    }


def _get_gradle_projects(gradle_root):
    if gradle_root.endswith("android-components"):
        return [c["name"] for c in get_components()]
    # TODO: Support focus and fenix
    raise NotImplementedError(f"Cannot find gradle projects for {gradle_root}")


def get_variant(build_type, build_name):
    all_variants = _fetch_all_variants()
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


@memoize
def _fetch_all_variants():
    output = _run_gradle_process('printVariants')
    content = _extract_content_from_command_output(output, prefix='variants: ')
    return json.loads(content)


def _run_gradle_process(gradle_command, **kwargs):
    gradle_properties = [
        f'-P{property_name}={value}'
        for property_name, value in kwargs.items()
    ]
    process = subprocess.Popen(
        ["./gradlew", "--no-daemon", "--quiet", gradle_command] + gradle_properties,
        stdout=subprocess.PIPE,
        universal_newlines=True,
        cwd=FOCUS_DIR,
    )
    output, err = process.communicate()
    exit_code = process.wait()

    if exit_code != 0:
        raise RuntimeError(f"Gradle command returned error: {exit_code}")

    return output


def _extract_content_from_command_output(output, prefix):
    variants_line = [line for line in output.split('\n') if line.startswith(prefix)][0]
    return variants_line.split(' ', 1)[1]
