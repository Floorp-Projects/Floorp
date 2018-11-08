# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Decision task for releases
"""

from __future__ import print_function
import argparse
import json
import os
import taskcluster

import lib.module_definitions
import lib.tasks

TASK_ID = os.environ.get('TASK_ID')
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


def fetch_build_task_artifacts(artifacts_info):
    artifacts = {}
    for info in artifacts_info:
        artifacts[info['artifact']] = {
            'type': 'file',
            'expires': taskcluster.stringDate(taskcluster.fromNow('1 year')),
            'path': info['path']
        }

    return artifacts


def generate_build_task(version, artifacts_info):
    checkout = ("git fetch origin --tags && "
                "git config advice.detachedHead false && "
                "git checkout {}".format(version))

    assemble_task = 'assembleRelease'
    artifacts = fetch_build_task_artifacts(artifacts_info)

    return taskcluster.slugId(), BUILDER.build_task(
        name="Android Components - Release ({})".format(version),
        description="Building and publishing release versions.",
        command=(checkout +
                 ' && ./gradlew --no-daemon clean test detekt ktlint '
                 + assemble_task +
                 ' docs uploadArchives zipMavenArtifacts'),
        features={
            "chainOfTrust": True
        },
        worker_type='gecko-focus',
        scopes=[],
        artifacts=artifacts,
    )


def generate_beetmover_task(build_task_id, version, artifact, artifact_name):
    version = version.lstrip('v')
    upstream_artifacts = [
        {
            "paths": [
                artifact
            ],
            "taskId": build_task_id,
            "taskType": "build",
            "zipExtract": True
        }
    ]
    scopes = [
        "project:mobile:android-components:releng:beetmover:bucket:maven-production",
        "project:mobile:android-components:releng:beetmover:action:push-to-maven"
    ]
    return taskcluster.slugId(), BUILDER.beetmover_task(
        name="Android Components - Publish Module :{} via beetmover".format(artifact_name),
        description="Publish release module {} to https://maven.mozilla.org".format(artifact_name),
        version=version,
        artifact_id=artifact_name,
        dependencies=[build_task_id],
        upstreamArtifacts=upstream_artifacts,
        scopes=scopes
    )


def populate_chain_of_trust_required_but_unused_files():
    # Thoses files are needed to keep chainOfTrust happy. However, they have no
    # need for android-components, at the moment. For more details, see:
    # https://github.com/mozilla-releng/scriptworker/pull/209/files#r184180585

    for file_names in ('actions.json', 'parameters.yml'):
        with open(file_names, 'w') as f:
            json.dump({}, f)    # Yaml is a super-set of JSON.


def release(version):
    queue = taskcluster.Queue({'baseUrl': 'http://taskcluster/queue/v1'})

    task_graph = {}
    artifacts_info = lib.module_definitions.from_gradle()

    build_task_id, build_task = generate_build_task(version, artifacts_info)
    lib.tasks.schedule_task(queue, build_task_id, build_task)

    task_graph[build_task_id] = {}
    task_graph[build_task_id]["task"] = queue.task(build_task_id)

    for info in artifacts_info:
        beetmover_task_id, beetmover_task = generate_beetmover_task(build_task_id, version, info['artifact'],
                                                                    info['name'])
        lib.tasks.schedule_task(queue, beetmover_task_id, beetmover_task)

        task_graph[beetmover_task_id] = {}
        task_graph[beetmover_task_id]["task"] = queue.task(beetmover_task_id)

    print(json.dumps(task_graph, indent=4, separators=(',', ': ')))

    task_graph_path = "task-graph.json"
    with open(task_graph_path, 'w') as f:
        json.dump(task_graph, f)

    populate_chain_of_trust_required_but_unused_files()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Create a release pipeline on taskcluster.')

    parser.add_argument('--version', dest="version",
                        action="store", help="git tag to build from")
    result = parser.parse_args()
    release(result.version)
