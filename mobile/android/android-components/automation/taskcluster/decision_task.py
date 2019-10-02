# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Decision task
"""

from __future__ import print_function

import argparse
import itertools
import os
import taskcluster
import sys

from lib.build_config import components_version, components, generate_snapshot_timestamp
from lib.tasks import TaskBuilder


AAR_EXTENSIONS = ('.aar', '.pom', '-sources.jar')
HASH_EXTENSIONS = ('', '.sha1', '.md5')


def _get_gradle_module_name(artifact_info):
    return ':{}'.format(artifact_info['name'])


def _get_release_gradle_tasks(module_name, is_snapshot):
    gradle_tasks = ' '.join(
        '{}:{}{}'.format(
            module_name,
            gradle_task,
            '' if is_snapshot else 'Release'
        )
        for gradle_task in ('assemble', 'test', 'lint')
    )

    rename_step = '{}:renameSnapshotArtifacts'.format(module_name) if is_snapshot else ''
    return "{} {}:publish {}".format(gradle_tasks, module_name, rename_step, module_name)


def _to_release_artifact(extension, version, component, timestamp=None):
    if timestamp:
        artifact_filename = '{}-{}-{}{}'.format(component['name'], version, timestamp, extension)
    else:
        artifact_filename = '{}-{}{}'.format(component['name'], version, extension)

    return {
        'taskcluster_path': 'public/build/{}'.format(artifact_filename),
        'build_fs_path': '/build/android-components/{}/build/maven/org/mozilla/components/{}/{}/{}'.format(
            component['path'],
            component['name'],
            version if not timestamp else version + '-SNAPSHOT',
            artifact_filename),
        'maven_destination': 'maven2/org/mozilla/components/{}/{}/{}'.format(
            component['name'],
            version if not timestamp else version + '-SNAPSHOT',
            artifact_filename,
        )
    }


def release(builder, components, is_snapshot, is_staging):
    version = components_version()

    tasks = []
    build_tasks_labels = {}
    wait_on_builds_label = 'Android Components - Barrier task to wait on other tasks to complete'

    timestamp = None
    if is_snapshot:
        timestamp = generate_snapshot_timestamp()

    for component in components:
        module_name = _get_gradle_module_name(component)
        build_task = builder.craft_build_task(
            module_name=module_name,
            gradle_tasks=_get_release_gradle_tasks(module_name, is_snapshot),
            subtitle='({}{})'.format(version, '-SNAPSHOT' if is_snapshot else ''),
            run_coverage=False,
            is_snapshot=is_snapshot,
            component=component,
            artifacts=[_to_release_artifact(extension + hash_extension, version, component, timestamp)
                       for extension, hash_extension in
                       itertools.product(AAR_EXTENSIONS, HASH_EXTENSIONS)],
            timestamp=timestamp,
        )
        tasks.append(build_task)
        # XXX Temporary hack to keep taskgraph happy about how dependencies are represented
        build_tasks_labels[build_task['label']] = build_task['label']

        sign_task = builder.craft_sign_task(
            build_task['label'], wait_on_builds_label, [_to_release_artifact(extension, version, component, timestamp)
                            for extension in AAR_EXTENSIONS],
            component['name'], is_staging,
        )
        tasks.append(sign_task)

        beetmover_build_artifacts = [_to_release_artifact(extension + hash_extension, version, component, timestamp)
                                     for extension, hash_extension in
                                     itertools.product(AAR_EXTENSIONS, HASH_EXTENSIONS)]
        beetmover_sign_artifacts = [_to_release_artifact(extension + '.asc', version, component, timestamp)
                                    for extension in AAR_EXTENSIONS]
        tasks.append(builder.craft_beetmover_task(
            build_task['label'], sign_task['label'], beetmover_build_artifacts, beetmover_sign_artifacts,
            component['name'], is_snapshot, is_staging
        ))

    tasks.append(builder.craft_barrier_task(wait_on_builds_label, build_tasks_labels))

    if is_snapshot:     # XXX These jobs perma-fail on release
        for craft_function in (builder.craft_detekt_task, builder.craft_ktlint_task, builder.craft_compare_locales_task):
            tasks.append(craft_function())

    return tasks
