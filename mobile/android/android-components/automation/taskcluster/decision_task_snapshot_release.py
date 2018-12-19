# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Decision task for triggering a snapshot release
"""

from __future__ import print_function
import json
import os
import taskcluster
import sys

import lib.module_definitions
import lib.tasks
import lib.util


TASK_ID = os.environ.get('TASK_ID')
REPO_URL = os.environ.get('MOBILE_HEAD_REPOSITORY')
BRANCH = os.environ.get('MOBILE_HEAD_BRANCH')
HEAD_REV = os.environ.get('MOBILE_HEAD_REV')

BUILDER = lib.tasks.TaskBuilder(
    task_id=TASK_ID,
    repo_url=os.environ.get('MOBILE_HEAD_REPOSITORY'),
    branch=os.environ.get('MOBILE_HEAD_BRANCH'),
    commit=HEAD_REV,
    owner="skaspari@mozilla.com",
    source='https://github.com/mozilla-mobile/android-components/raw/{}/.taskcluster.yml'.format(HEAD_REV),
    scheduler_id=os.environ.get('SCHEDULER_ID'),
)


def generate_build_task(info):
    checkout = ("export TERM=dumb && git fetch {} {} && "
                "git config advice.detachedHead false && "
                "git checkout {} ".format(REPO_URL, BRANCH, HEAD_REV))
    artifacts = {
        info["artifact"]: {
            'type': 'file',
            'expires': taskcluster.stringDate(taskcluster.fromNow('1 year')),
            'path': info['path']
        }
    }
    module_name = ":{}".format(info['name'])

    return taskcluster.slugId(), BUILDER.build_task(
        name='Android Components - Module {}'.format(module_name),
        description='Building and testing module {}'.format(module_name),
        command=(checkout +
                 ' && ./gradlew --no-daemon clean ' +
                 "-Psnapshot " +
                 " ".join(["{}:{}".format(module_name, gradle_task) for gradle_task in ['assemble', 'test', 'lint']])  +
                 " uploadArchives zipMavenArtifacts"),
        features={
            "chainOfTrust": True,
            "taskclusterProxy": True
        },
        worker_type='gecko-focus',
        scopes = [],
        artifacts=artifacts,
    )


def generate_beetmover_task(build_task_id, info, version):
    module_name = ":{}".format(info['name'])
    upstream_artifacts = [
        {
            "paths": [
                info['artifact'],
            ],
            "taskId": build_task_id,
            "taskType": "build",
            "zipExtract": True
        }
    ]
    # TODO: switch to prod bucket once staging works smoothly
    scopes = [
        "project:mobile:android-components:releng:beetmover:bucket:maven-snapshot-staging",
        "project:mobile:android-components:releng:beetmover:action:push-to-maven"
    ]
    # TODO: switch to prod mobile-beetmoverwoerk-v1
    return taskcluster.slugId(), BUILDER.beetmover_task(
        name="Android Components - Publish Snapshot Module {} via beetmover".format(module_name),
        description="Publish snapshot release module {} to https://maven.mozilla.org".format(module_name),
        version=version,
        artifact_id=info['name'],
        dependencies=[build_task_id],
        upstreamArtifacts=upstream_artifacts,
        scopes=scopes,
        worker_type='mobile-beetmover-dev',
        is_snapshot=True
    )


def generate_raw_task(name, description, command_to_run):
    checkout = ("export TERM=dumb && git fetch {} {} && "
                "git config advice.detachedHead false && "
                "git checkout {} && ".format(REPO_URL, BRANCH, HEAD_REV))
    return taskcluster.slugId(), BUILDER.raw_task(
        name=name,
        description=description,
        command=(checkout +
                 command_to_run)
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


def snapshot_release():
    queue = taskcluster.Queue({ 'baseUrl': 'http://taskcluster/queue/v1' })

    task_graph = {}

    version = lib.module_definitions.get_version_from_gradle()
    artifacts_info = [info for info in lib.module_definitions.from_gradle() if info['shouldPublish']]
    if len(artifacts_info) == 0:
        print("Could not get module names from gradle")
        sys.exit(2)


    for info in artifacts_info:
        build_task_id, build_task = generate_build_task(info)
        lib.tasks.schedule_task(queue, build_task_id, build_task)

        task_graph[build_task_id] = {}
        task_graph[build_task_id]["task"] = queue.task(build_task_id)

        beetmover_task_id, beetmover_task = generate_beetmover_task(build_task_id, info, version)
        lib.tasks.schedule_task(queue, beetmover_task_id, beetmover_task)

        task_graph[beetmover_task_id] = {}
        task_graph[beetmover_task_id]["task"] = queue.task(beetmover_task_id)

    detekt_task_id, detekt_task = generate_detekt_task()
    lib.tasks.schedule_task(queue, detekt_task_id, detekt_task)
    task_graph[detekt_task_id] = {}
    task_graph[detekt_task_id]["task"] = queue.task(detekt_task_id)

    ktlint_task_id, ktlint_task = generate_ktlint_task()
    lib.tasks.schedule_task(queue, ktlint_task_id, ktlint_task)
    task_graph[ktlint_task_id] = {}
    task_graph[ktlint_task_id]["task"] = queue.task(ktlint_task_id)

    comp_locales_task_id, comp_locales_task = generate_compare_locales_task()
    lib.tasks.schedule_task(queue, comp_locales_task_id, comp_locales_task)
    task_graph[comp_locales_task_id] = {}
    task_graph[comp_locales_task_id]["task"] = queue.task(comp_locales_task_id)

    print(json.dumps(task_graph, indent=4, separators=(',', ': ')))

    task_graph_path = "task-graph.json"
    with open(task_graph_path, 'w') as f:
        json.dump(task_graph, f)

    lib.util.populate_chain_of_trust_required_but_unused_files()


if __name__ == "__main__":
    snapshot_release()
