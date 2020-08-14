# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import logging
import os
import re
import subprocess

from copy import deepcopy
from taskgraph.loader.transform import loader as base_loader
from taskgraph.util.memoize import memoize
from taskgraph.util.taskcluster import get_session
from taskgraph.util.templates import merge

from ..files_changes import get_files_changed_pr, get_files_changed_push
from ..build_config import get_components


logger = logging.getLogger(__name__)

# Per https://github.com/git/git/blob/dc04167d378fb29d30e1647ff6ff51dd182bc9a3/t/oid-info/hash-info#L7-L8
_GIT_ZERO_HASHES = (
    "0000000000000000000000000000000000000000", # sha1
    "0000000000000000000000000000000000000000000000000000000000000000", # sha256
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
    return set(["-".join(f.split("/")[1:3]) for f in files_changed if f.startswith("components")])


cached_deps = {}

# Gradle is expensive to run. Cache the results for each component.
@memoize
def get_deps_for_component(component):
    """Return the full list of local upstream dependencies of a component."""
    deps = set()
    # Parsing output like this is not ideal but bhearsum couldn't find a way
    # to get the dependencies printed in a better format. If we could convince
    # gradle to spit out JSON that would be much better.
    # This is filed as https://github.com/mozilla-mobile/android-components/issues/7814
    for line in subprocess.check_output(['./gradlew', '%s:dependencies' % component, '--configuration', 'implementation']).splitlines():
        if line.startswith("+--- project"):
            deps.add(line.split(" ")[2])

    cached_deps[component] = deps
    # So far, we only have the direct local upstream dependencies. We need to look at
    # each of those to find _their_ upstreams as well.
    for d in deps.copy():
        deps.update(get_deps_for_component(d))

    return deps

def get_affected_components(files_changed, files_affecting_components):
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
    # find their dependencies.
    for c in affected_components.copy():
        affected_components.update(get_deps_for_component(c))

    return affected_components


def loader(kind, path, config, params, loaded_tasks):
    # Build everything unless we have an optimization strategy (defined below).
    files_changed = []
    affected_components = ALL_COMPONENTS

    if params["tasks_for"] == "github-pull-request":
        logger.info("Processing pull request %s" % params["pull_request_number"])
        files_changed = get_files_changed_pr(params["base_repository"], params["pull_request_number"])
        affected_components = get_affected_components(files_changed, config.get("files-affecting-components"))
    elif params["tasks_for"] == "github-push":
        if params["base_rev"] in _GIT_ZERO_HASHES:
            logger.warn("base_rev is a zero hash, meaning there is no previous push. Building every component...")
        else:
            logger.info("Processing push for commit range %s -> %s" % (params["base_rev"], params["head_rev"]))
            files_changed = get_files_changed_push(params["base_repository"], params["base_rev"], params["head_rev"])
            affected_components = get_affected_components(files_changed, config.get("files-affecting-components"))

    logger.info("Files changed: %s" % " ".join(files_changed))
    if affected_components is ALL_COMPONENTS:
        logger.info("Affected components: ALL")
    else:
        logger.info("Affected components: %s" % " ".join(affected_components))

    not_for_components = config.get("not-for-components", [])
    jobs = {
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
    # Filter away overridden jobs that we wouldn't build anyways to avoid ending up with
    # partial job entries.
    overridden_jobs = {k: v for k, v in config.pop('overriden-jobs', {}).items() if affected_components is ALL_COMPONENTS or k in jobs.keys()}
    jobs = merge(jobs, overridden_jobs)

    config['jobs'] = jobs

    return base_loader(kind, path, config, params, loaded_tasks)
