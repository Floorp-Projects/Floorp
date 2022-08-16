# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import os
import re
import subprocess

from collections import defaultdict
from copy import deepcopy
from taskgraph.files_changed import get_changed_files
from taskgraph.loader.transform import loader as base_loader
from taskgraph.util.taskcluster import get_session
from taskgraph.util.templates import merge

from ..build_config import get_components, ANDROID_COMPONENTS_DIR


logger = logging.getLogger(__name__)

CONFIGURATIONS_WITH_DEPENDENCIES = (
    "api",
    "compileOnly",
    "implementation",
    "testImplementation"
)
ALL_COMPONENTS = object()

def get_components_changed(files_changed):
    """Translate a list of files changed into a list of components. Eg:
        [
            "components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/GeckoLoginStorageDelegate.kt",
            "components/service/sync-logins/src/test/java/mozilla/components/service/sync/logins/GeckoLoginStorageDelegateTest.kt",
        ]
        ->
        {service-sync-logins}
    """
    return {"-".join(f.split("/")[1:3]) for f in files_changed if f.startswith("components")}


cached_deps = {}

def get_upstream_deps_for_components(components):
    """Return the full list of local upstream dependencies of a component."""
    component_dependencies = defaultdict(set)

    for configuration in CONFIGURATIONS_WITH_DEPENDENCIES:
        logger.info("Looking for dependencies in '%s' configuration" % configuration)

        cmd = ["./gradlew"]
        # This is eventually going to fail if there's ever enough components to make the command line
        # too long. If that happens, we'll need to split this list up and run gradle more than once.
        for c in sorted(components):
            cmd.extend(["%s:dependencies" % c, "--configuration", configuration])
        # Parsing output like this is not ideal but bhearsum couldn't find a way
        # to get the dependencies printed in a better format. If we could convince
        # gradle to spit out JSON that would be much better.
        # This is filed as https://github.com/mozilla-mobile/android-components/issues/7814
        current_component = None
        for line in subprocess.check_output(cmd, universal_newlines=True, cwd=ANDROID_COMPONENTS_DIR).splitlines():
            # If we find the start of a new component section, update our tracking variable
            if line.startswith("Project"):
                current_component = line.split(":")[1].strip("'")

            # If we find a new local dependency, add it.
            if line.startswith("+--- project") or line.startswith(r"\--- project"):
                component_dependencies[current_component].add(line.split(" ")[2])

    return [(k, sorted(component_dependencies[k])) for k in sorted(component_dependencies)]


def get_affected_components(files_changed, files_affecting_components, upstream_component_dependencies, downstream_component_dependencies):
    affected_components = set()

    # First, find the list of changed components
    for c in get_components_changed(files_changed):
        affected_components.add(c)

    # Then, look for any other affected components based on the files changed.
    for pattern, components in files_affecting_components.items():
        if any([re.match(pattern, f) for f in files_changed]):
            # Some file changes may necessitate rebuilding _all_ components.
            # In this we can just return immediately.
            if components == "all-components":
                return ALL_COMPONENTS

            affected_components.update(components)

    # Finally, go through all of the affected components and recursively
    # find their upstream and downstream dependencies.
    for c in affected_components.copy():
        if upstream_component_dependencies[c]:
            logger.info("Adding direct upstream dependencies for '{}': {}".format(c, " ".join(sorted(upstream_component_dependencies[c]))))
            affected_components.update(upstream_component_dependencies[c])
        if downstream_component_dependencies[c]:
            logger.info("Adding direct downstream dependencies for '{}': {}".format(c, " ".join(sorted(downstream_component_dependencies[c]))))
            affected_components.update(downstream_component_dependencies[c])

    return affected_components


def loader(kind, path, config, params, loaded_tasks):
    # Build everything unless we have an optimization strategy (defined below).
    files_changed = []
    affected_components = ALL_COMPONENTS
    upstream_component_dependencies = defaultdict(set)
    downstream_component_dependencies = defaultdict(set)

    for component, deps in get_upstream_deps_for_components([c["name"] for c in get_components()]):
        if deps:
            logger.info(f"Found direct upstream dependencies for component '{component}': {deps}")
        else:
            logger.info("No direct upstream dependencies found for component '%s'" % component)
        upstream_component_dependencies[component] = deps
        for d in deps:
            downstream_component_dependencies[d].add(component)

    if params["head_ref"] == "refs/heads/main":
        # Disable the affected_components optimization to make sure we execute all tests to get
        # a complete code coverage report for pushes to 'main'.
        # See https://github.com/mozilla-mobile/android-components/issues/9382#issuecomment-760506327
        logger.info("head_ref is refs/heads/main. Building every component...")
        affected_components = ALL_COMPONENTS
    else:
        files_changed = get_changed_files(params["head_repository"], params["head_rev"], params["base_rev"])
        affected_components = get_affected_components(files_changed, config.get("files-affecting-components"), upstream_component_dependencies, downstream_component_dependencies)

    logger.info("Files changed: %s" % " ".join(files_changed))
    if affected_components is ALL_COMPONENTS:
        logger.info("Affected components: ALL")
    else:
        logger.info("Affected components: %s" % " ".join(sorted(affected_components)))

    not_for_components = config.get("not-for-components", [])
    tasks = {
        '{}{}'.format(
            '' if build_type == 'regular' else build_type + '-',
            component['name']
        ): {
            'attributes': {
                'build-type': build_type,
                'component': component['name'],
            }
        }
        for component in get_components()
        for build_type in ('regular', 'nightly', 'release')
        if (
            (affected_components is ALL_COMPONENTS or component['name'] in affected_components)
            and component['name'] not in not_for_components
            and (component['shouldPublish'] or build_type == 'regular')
        )
    }
    # Filter away overridden tasks that we wouldn't build anyways to avoid ending up with
    # partial task entries.
    overridden_tasks = {k: v for k, v in config.pop('overriden-tasks', {}).items() if affected_components is ALL_COMPONENTS or k in tasks.keys()}
    tasks = merge(tasks, overridden_tasks)

    config['tasks'] = tasks

    return base_loader(kind, path, config, params, loaded_tasks)
