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

TASK_ID = os.environ.get('TASK_ID')

BUILDER = lib.tasks.TaskBuilder(
    task_id=TASK_ID,
    repo_url=os.environ.get('GITHUB_HEAD_REPO_URL'),
    branch=os.environ.get('GITHUB_HEAD_BRANCH'),
    commit=os.environ.get('GITHUB_HEAD_SHA'),
    owner="skaspari@mozilla.com",
    source="https://github.com/mozilla-mobile/focus-android/tree/master/tools/taskcluster"
)

def generate_build_task(tag):
    return taskcluster.slugId(), BUILDER.build_task(
        name="(Focus for Android) Build task",
        description="Build Focus/Klar from source code.",
        command=("git fetch origin && git checkout %s" % (tag) +
                 ' && python tools/l10n/filter-release-translations.py'
                 ' && python tools/taskcluster/get-adjust-token.py'
                 ' && python tools/taskcluster/get-sentry-token.py'
                 ' && ./gradlew --no-daemon clean test assembleRelease'),
        features = {
            "chainOfTrust": True
        },
        artifacts = {
			"public/focus.apk": {
				"type": "file",
				"path": "/opt/focus-android/app/build/outputs/apk/focusWebviewUniversal/release/app-focus-webview-universal-release-unsigned.apk",
				"expires": taskcluster.stringDate(taskcluster.fromNow('1 month'))
			},
            "public/klar.apk": {
				"type": "file",
				"path": "/opt/focus-android/app/build/outputs/apk/klarWebviewUniversal/release/app-klar-webview-universal-release-unsigned.apk",
				"expires": taskcluster.stringDate(taskcluster.fromNow('1 month'))
			}
		},
        worker_type='gecko-focus',
        scopes=[
            "secrets:get:project/focus/tokens"
        ])

def generate_signing_task(build_task_id):
    return taskcluster.slugId(), BUILDER.build_signing_task(
        build_task_id,
        name="(Focus for Android) Signing task",
        description="Sign release builds of Focus/Klar",
        apks=[
            "public/focus.apk",
            "public/klar.apk"
        ],
        scopes = [
            "project:mobile:focus:releng:signing:cert:release-signing",
            "project:mobile:focus:releng:signing:format:focus-jar"
        ]
    )

def generate_push_task(signing_task_id, track, commit):
    return taskcluster.slugId(), BUILDER.build_push_task(
        signing_task_id,
        name="(Focus for Android) Push task",
        description="Upload signed release builds of Focus/Klar to Google Play",
        apks=[
            "public/focus.apk",
            "public/klar.apk"
        ],
        scopes=[
            "project:mobile:focus:releng:googleplay:product:focus"
        ],
        track = track,
        commit = commit
    )

def release(track, commit, tag):
    queue = taskcluster.Queue({ 'baseUrl': 'http://taskcluster/queue/v1' })

    task_graph = {}

    build_task_id, build_task = generate_build_task(tag)
    lib.tasks.schedule_task(queue, build_task_id, build_task)

    task_graph[build_task_id] = {}
    task_graph[build_task_id]["task"] = queue.task(build_task_id)

    sign_task_id, sign_task = generate_signing_task(build_task_id)
    lib.tasks.schedule_task(queue, sign_task_id, sign_task)

    task_graph[sign_task_id] = {}
    task_graph[sign_task_id]["task"] = queue.task(sign_task_id)

    push_task_id, push_task = generate_push_task(sign_task_id, track, commit)
    lib.tasks.schedule_task(queue, push_task_id, push_task)

    task_graph[push_task_id] = {}
    task_graph[push_task_id]["task"] = queue.task(push_task_id)

    print json.dumps(task_graph, indent=4, separators=(',', ': '))

    task_graph_path = "task-graph.json"
    with open(task_graph_path, 'w') as token_file:
        token_file.write(json.dumps(task_graph))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Create a release pipeline (build, sign, publish) on taskcluster.')

    parser.add_argument('--track', dest="track", action="store", choices=['internal', 'alpha'], help="", required=True)
    parser.add_argument('--commit', dest="commit", action="store_true", help="commit the google play transaction")
    parser.add_argument('--tag', dest="tag", action="store", help="git tag to build from")

    result = parser.parse_args()

    release(result.track, result.commit, result.tag)
