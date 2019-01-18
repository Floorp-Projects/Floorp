# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Decision task for releases and snapshots
"""

from __future__ import print_function
import argparse
import json
import os
import taskcluster
import sys

import lib.build_config
import lib.tasks
import lib.util

TASK_ID = os.environ.get('TASK_ID')
HEAD_REV = os.environ.get('MOBILE_HEAD_REV')
BRANCH = os.environ.get('MOBILE_HEAD_BRANCH')
REPO_URL = os.environ.get('MOBILE_HEAD_REPOSITORY')

BUILDER = lib.tasks.TaskBuilder(
    task_id=TASK_ID,
    repo_url=os.environ.get('MOBILE_HEAD_REPOSITORY'),
    branch=BRANCH,
    commit=HEAD_REV,
    owner="skaspari@mozilla.com",
    source='{}/raw/{}/.taskcluster.yml'.format(REPO_URL, HEAD_REV),
    scheduler_id=os.environ.get('SCHEDULER_ID'),
    build_worker_type=os.environ.get('BUILD_WORKER_TYPE'),
    beetmover_worker_type=os.environ.get('BEETMOVER_WORKER_TYPE'),
    tasks_priority=os.environ.get('TASKS_PRIORITY'),
)

BUILD_GRADLE_TASK_NAMES = ('assemble', 'test', 'lint')


def _get_gradle_module_name(name):
    return ':{}'.format(name)


def generate_build_task(version, artifact_info, is_snapshot):
    checkout = ("export TERM=dumb && git fetch {} {} --tags && "
                "git config advice.detachedHead false && "
                "git checkout {}".format(REPO_URL, BRANCH, HEAD_REV))

    artifacts = {
        artifact_info['artifact']: {
            'type': 'file',
            'expires': taskcluster.stringDate(taskcluster.fromNow('1 year')),
            'path': artifact_info['path']
        }
    }

    snapshot_flag = '-Psnapshot ' if is_snapshot else ''
    module_name = _get_gradle_module_name(artifact_info['name'])
    gradle_tasks_for_this_module_only = ' '.join(
        '{}:{}{}'.format(
            module_name,
            gradle_task,
            '' if is_snapshot else 'Release'
        )
        for gradle_task in BUILD_GRADLE_TASK_NAMES
    )

    return taskcluster.slugId(), BUILDER.craft_build_ish_task(
        name='Android Components - Module {} ({})'.format(module_name, version),
        description='Building and testing module {}'.format(module_name),
        command=(
            checkout +
            ' && ./gradlew --no-daemon clean ' +
            snapshot_flag + gradle_tasks_for_this_module_only +
            ' uploadArchives zipMavenArtifacts'
        ),
        features={
            'chainOfTrust': True,
        },
        scopes=[],
        artifacts=artifacts
    )


def generate_beetmover_task(build_task_id, wait_on_builds_task_id, version, artifact, artifact_name, is_snapshot, is_staging):
    version = version.lstrip('v')
    module_name = _get_gradle_module_name(artifact_name)
    upstream_artifacts = [{
        'paths': [artifact],
        'taskId': build_task_id,
        'taskType': 'build',
        'zipExtract': True,
    }]

    if is_snapshot:
        if is_staging:
            bucket_name = 'maven-snapshot-staging'
            bucket_public_url = 'https://maven-snapshots.stage.mozaws.net/'
        else:
            bucket_name = 'maven-snapshot-production'
            bucket_public_url = 'https://snapshots.maven.mozilla.org/'
    else:
        if is_staging:
            bucket_name = 'maven-staging'
            bucket_public_url = 'https://maven-default.stage.mozaws.net/'
        else:
            bucket_name = 'maven-production'
            bucket_public_url = 'https://maven.mozilla.org/'

    scopes = [
        "project:mobile:android-components:releng:beetmover:bucket:{}".format(bucket_name),
        "project:mobile:android-components:releng:beetmover:action:push-to-maven",
    ]
    return taskcluster.slugId(), BUILDER.craft_beetmover_task(
        name="Android Components - Publish Module :{} via beetmover".format(artifact_name),
        description="Publish release module {} to {}".format(artifact_name, bucket_public_url),
        version=version,
        artifact_id=artifact_name,
        dependencies=[build_task_id, wait_on_builds_task_id],
        upstreamArtifacts=upstream_artifacts,
        scopes=scopes,
        is_snapshot=is_snapshot
    )


def generate_raw_task(name, description, command_to_run):
    checkout = ("export TERM=dumb && git fetch {} {} && "
                "git config advice.detachedHead false && "
                "git checkout {} && ".format(REPO_URL, BRANCH, HEAD_REV))
    return taskcluster.slugId(), BUILDER.craft_build_ish_task(
        name=name,
        description=description,
        command=(checkout + command_to_run)
    )


def generate_detekt_task():
    return generate_raw_task(
        name='Android Components - detekt',
        description='Running detekt over all modules',
        command_to_run='./gradlew --no-daemon clean detekt')


def generate_ktlint_task():
    return generate_raw_task(
        name='Android Components - ktlint',
        description='Running ktlint over all modules',
        command_to_run='./gradlew --no-daemon clean ktlint')


def generate_compare_locales_task():
    return generate_raw_task(
        name='Android Components - compare-locales',
        description='Validate strings.xml with compare-locales',
        command_to_run='pip install "compare-locales>=4.0.1,<5.0" && compare-locales --validate l10n.toml .')


def generate_wait_on_builds_task(dependencies):
    return BUILDER.craft_build_ish_task(
        name='Android Components - Wait on all builds to be completed',
        description='Dummy tasks that ensures all builds are correctly done before publishing them',
        dependencies=dependencies,
        command="exit 0"
    )


def generate_ordered_groups_of_tasks(artifacts_info, version, is_snapshot, is_staging):
    build_tasks = {}
    wait_on_builds_tasks = {}
    beetmover_tasks = {}
    other_tasks = {}
    wait_on_builds_task_id = taskcluster.slugId()

    for artifact_info in artifacts_info:
        build_task_id, build_task_definition = generate_build_task(version, artifact_info, is_snapshot)
        build_tasks[build_task_id] = build_task_definition

        beetmover_tasks_id, beetmover_task_definition = generate_beetmover_task(
            build_task_id, wait_on_builds_task_id, version, artifact_info['artifact'],
            artifact_info['name'], is_snapshot, is_staging
        )
        beetmover_tasks[beetmover_tasks_id] = beetmover_task_definition

    wait_on_builds_tasks[wait_on_builds_task_id] = generate_wait_on_builds_task(build_tasks.keys())

    if is_snapshot:     # XXX These jobs perma-fail on release
        for generate_function in (generate_detekt_task, generate_ktlint_task, generate_compare_locales_task):
            task_id, task_definition = generate_function()
            other_tasks[task_id] = task_definition

    return (build_tasks, wait_on_builds_tasks, beetmover_tasks, other_tasks)


def release(version, is_snapshot, is_staging):
    artifacts_info = [info for info in lib.build_config.module_definitions() if info['shouldPublish']]
    if len(artifacts_info) == 0:
        print("Could not get module names from gradle")
        sys.exit(2)

    version = lib.build_config.components_version() if version is None else version

    ordered_groups_of_tasks = generate_ordered_groups_of_tasks(artifacts_info, version, is_snapshot, is_staging)
    full_task_graph = lib.tasks.schedule_task_graph(ordered_groups_of_tasks)

    lib.util.populate_chain_of_trust_task_graph(full_task_graph)
    lib.util.populate_chain_of_trust_required_but_unused_files()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Create a release pipeline on taskcluster.')

    parser.add_argument(
        '--version', action="store", help="git tag to build from",
        default=None
    )
    parser.add_argument(
        '--snapshot', action="store_true",
        help="Create snapshot artifacts and upload them onto the snapshot repository"
    )
    parser.add_argument(
        '--staging', action="store_true",
        help="Perform a staging build. Artifacts are uploaded on either "
             "https://maven-default.stage.mozaws.net/ or https://maven-snapshots.stage.mozaws.net/"
    )
    result = parser.parse_args()
    release(result.version, result.snapshot, result.staging)
