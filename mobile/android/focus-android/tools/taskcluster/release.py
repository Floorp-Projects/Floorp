# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script is executed on taskcluster whenever we want to release builds
to Google Play. It will schedule the taskcluster tasks for building,
signing and uploading a release.
"""

import argparse
import json
import os
import taskcluster
import lib.tasks

from lib.gradle import get_variant

TASK_ID = os.environ.get('TASK_ID')
SCHEDULER_ID = os.environ.get('SCHEDULER_ID')
HEAD_REV = os.environ.get('MOBILE_HEAD_REV')

BUILDER = lib.tasks.TaskBuilder(
    task_id=TASK_ID,
    repo_url=os.environ.get('MOBILE_HEAD_REPOSITORY'),
    branch=os.environ.get('MOBILE_HEAD_BRANCH'),
    commit=HEAD_REV,
    owner="skaspari@mozilla.com",
    source='https://github.com/mozilla-mobile/focus-android/raw/{}/.taskcluster.yml'.format(HEAD_REV),
    scheduler_id=SCHEDULER_ID,
)

def generate_build_task(artifacts, tag):

    checkout = "git fetch origin && git reset --hard origin/master" if tag is None else "git fetch origin && git checkout %s" % (tag)

    assemble_task = 'assembleNightly'

    if tag:
        # Non-tagged (nightly) builds should contain all languages
        checkout = checkout + ' && python tools/l10n/filter-release-translations.py'
        assemble_task = 'assembleRelease'

    return taskcluster.slugId(), BUILDER.build_task(
        name="(Focus for Android) Build task",
        description="Build Focus/Klar from source code.",
        command=(checkout +
                 ' && python tools/taskcluster/get-adjust-token.py'
                 ' && python tools/taskcluster/get-sentry-token.py'
                 ' && ./gradlew --no-daemon clean test ' + assemble_task),
        features = {
            "chainOfTrust": True
        },
        artifacts = artifacts,
        worker_type='gecko-focus',
        scopes=[
            "secrets:get:project/focus/tokens"
        ])

def generate_signing_task(build_task_id, upstream_artifacts, tag):

    routes = []

    signing_format = 'autograph_focus'

    scopes = [
        "project:mobile:focus:releng:signing:cert:release-signing",
        "project:mobile:focus:releng:signing:format:{}".format(signing_format),
    ]

    if tag:
        index = "index.project.mobile.focus.release.latest"
        routes.append(index)
        scopes.append("queue:route:" + index)
    else:
        index = "index.project.mobile.focus.nightly.latest"
        routes.append(index)
        scopes.append("queue:route:" + index)

    return taskcluster.slugId(), BUILDER.build_signing_task(
        build_task_id,
        name="(Focus for Android) Signing task",
        description="Sign release builds of Focus/Klar",
        signing_format=signing_format,
        apks=upstream_artifacts,
        scopes=scopes,
        routes=routes
    )

def generate_push_task(signing_task_id, upstream_artifacts, channel, commit):

    print upstream_artifacts

    return taskcluster.slugId(), BUILDER.build_push_task(
        signing_task_id,
        name="(Focus for Android) Push task",
        description="Upload signed release builds of Focus/Klar to Google Play",
        apks=upstream_artifacts,
        scopes=[
            "project:mobile:focus:releng:googleplay:product:focus"
        ],
        channel = channel,
        commit = commit
    )

def populate_chain_of_trust_required_but_unused_files():
    # Thoses files are needed to keep chainOfTrust happy. However, they have no need for Firefox
    # Focus, at the moment. For more details, see:
    # https://github.com/mozilla-releng/scriptworker/pull/209/files#r184180585

    for file_names in ('actions.json', 'parameters.yml'):
        with open(file_names, 'w') as f:
            json.dump({}, f)    # Yaml is a super-set of JSON.


def release(variants, channel, commit, tag):
    queue = taskcluster.Queue({ 'baseUrl': 'http://taskcluster/queue/v1' })

    task_graph = {}
    artifacts = []
    upstream_artifacts = []
    for variant in variants:
        artifacts.extend(variant.artifacts())
        upstream_artifacts.extend(variant.upstream_artifacts())

    build_task_id, build_task = generate_build_task(artifacts, tag)
    lib.tasks.schedule_task(queue, build_task_id, build_task)

    task_graph[build_task_id] = {}
    task_graph[build_task_id]["task"] = queue.task(build_task_id)

    sign_task_id, sign_task = generate_signing_task(build_task_id, upstream_artifacts, tag)
    lib.tasks.schedule_task(queue, sign_task_id, sign_task)

    task_graph[sign_task_id] = {}
    task_graph[sign_task_id]["task"] = queue.task(sign_task_id)

    push_task_id, push_task = generate_push_task(sign_task_id, upstream_artifacts, channel, commit)
    lib.tasks.schedule_task(queue, push_task_id, push_task)

    task_graph[push_task_id] = {}
    task_graph[push_task_id]["task"] = queue.task(push_task_id)

    print json.dumps(task_graph, indent=4, separators=(',', ': '))

    task_graph_path = "task-graph.json"
    with open(task_graph_path, 'w') as f:
        json.dump(task_graph, f)

    populate_chain_of_trust_required_but_unused_files()



if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Create a release pipeline (build, sign, publish) on taskcluster.')

    parser.add_argument('--channel', dest="channel", action="store", choices=['internal', 'alpha', 'nightly'], help="", required=True)
    parser.add_argument('--commit', dest="commit", action="store_true", help="commit the google play transaction")
    parser.add_argument('--tag', dest="tag", action="store", help="git tag to build from")

    result = parser.parse_args()

    variants = []
    for product in ('focus', 'klar'):
        build_type = "release" if result.channel == "alpha" else result.channel
        my_variant = get_variant(build_type, product)
        variants.append(my_variant)

    release(variants, result.channel, result.commit, result.tag)
