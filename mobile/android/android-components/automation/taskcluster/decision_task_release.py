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
import yaml

import lib.tasks

TASK_ID = os.environ.get('TASK_ID')

BUILDER = lib.tasks.TaskBuilder(
    task_id=TASK_ID,
    repo_url=os.environ.get('GITHUB_HEAD_REPO_URL'),
    branch=os.environ.get('GITHUB_HEAD_BRANCH'),
    commit=os.environ.get('GITHUB_HEAD_SHA'),
    owner="skaspari@mozilla.com",
    source="https://github.com/mozilla-mobile/android-components.git"
)


def load_artifacts_manifest():
    return yaml.safe_load(open('automation/taskcluster/artifacts.yml', 'r'))


def fetch_build_task_artifacts():
    artifacts_info = load_artifacts_manifest()
    artifacts = {}
    for artifact, info in artifacts_info.items():
        artifacts[artifact] = {
            'type': 'file',
            'expires': taskcluster.stringDate(taskcluster.fromNow('1 year')),
            'path': info['path']
        }

    return artifacts


def generate_build_task(version):
    checkout = ("git fetch origin --tags && "
                "git config advice.detachedHead false && "
                "git checkout {}".format(version))

    assemble_task = 'assembleRelease'
    scopes = [
        "secrets:get:project/android-components/publish",
    ]
    artifacts = fetch_build_task_artifacts()

    return taskcluster.slugId(), BUILDER.build_task(
        name="Android Components - Release ({})".format(version),
        description="Building and publishing release versions.",
        command=(checkout +
                 ' && ./gradlew --no-daemon clean test detektCheck ktlint '
                 + assemble_task +
                 ' docs uploadArchives zipMavenArtifacts'),
        # TODO: add `./gradlew bintrayUpload --debug` too when fully ready
        features={
            "chainOfTrust": True
        },
        worker_type='gecko-focus',
        scopes=scopes,
        artifacts=artifacts,
    )


def generate_massager_task(build_task_id, massager_task_id,
                           artifacts_info, version):
    command = ("git fetch origin --tags && "
               "git config advice.detachedHead false && "
               "git checkout {} && "
               "apt-get install -y python3-pip && "
               "pip3 install scriptworker && "
               "python3 automation/taskcluster/release/convert_group_and_artifact_ids.py {}".format(version, massager_task_id))
    scopes = []
    upstreamZip = []
    for artifact, info in artifacts_info.items():
        new_artifact_id = None if info['name'] == info['old_artifact_id'] else info['name']
        new_group_id = info['new_group_id'] if new_artifact_id else None
        upstreamZip.append({
                "path": artifact,
                "taskId": build_task_id,
                "newGroupId": new_group_id,
                "newArtifactId": new_artifact_id,
        })

    return BUILDER.massager_task(
        name="Android Components - Massager",
        description="Task to massage the group/artifact ids",
        dependencies=[build_task_id],
        command=command,
        scopes=scopes,
        features={
            "chainOfTrust": True
        },
        artifacts={
            "public/build": {
                "expires": taskcluster.stringDate(taskcluster.fromNow('1 year')),
                "path": "/build/android-components/work_dir/public/build",
                "type": "directory"
            }
        },
        upstreamZip=upstreamZip,
    )


def generate_beetmover_task(build_task_id, version, artifact, info):
    version = version.lstrip('v')
    upstreamArtifacts = [
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
        name="Android Components - Publish Module :{} via beetmover".format(info['name']),
        description="Publish release module {} to https://maven.mozilla.org".format(info['name']),
        version=version,
        artifact_id=info['name'],
        dependencies=[build_task_id],
        upstreamArtifacts=upstreamArtifacts,
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

    build_task_id, build_task = generate_build_task(version)
    lib.tasks.schedule_task(queue, build_task_id, build_task)

    task_graph[build_task_id] = {}
    task_graph[build_task_id]["task"] = queue.task(build_task_id)

    artifacts_info = load_artifacts_manifest()
    # XXX: temporary hack: along with the release pipeline rewrite changes for
    # https://github.com/mozilla-mobile/android-components/issues/368 have also
    # been taken into consideration. But changing the `artifact_id` solely here
    # will break Cot on beetmover tasks as the build task still generates them
    # with the `artifact_id` that's baked within the `build.gradle` of each of
    # the components. Therefore, we inject a temporary task in between Build
    # task and beetmover tasks that's to massage the ids according to the new
    # ones. That means, downloading the zip archives from the Build task,
    # extracting, renaming files and re-zipping things back to feed Beetmover
    # job.
    massager_task_id = taskcluster.slugId()
    massager_task = generate_massager_task(build_task_id, massager_task_id,
                                           artifacts_info, version)
    lib.tasks.schedule_task(queue, massager_task_id, massager_task)

    task_graph[massager_task_id] = {}
    task_graph[massager_task_id]["task"] = queue.task(massager_task_id)

    for artifact, info in artifacts_info.items():
        beetmover_task_id, beetmover_task = generate_beetmover_task(massager_task_id, version, artifact, info)
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
