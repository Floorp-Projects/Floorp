# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Decision task for nightly releases.
"""

from __future__ import print_function

import argparse
import arrow
import json
import lib.tasks
import os
import taskcluster

TASK_ID = os.environ.get('TASK_ID')
SCHEDULER_ID = os.environ.get('SCHEDULER_ID')
GITHUB_HTTP_REPOSITORY = os.environ.get('MOBILE_HEAD_REPOSITORY')
HEAD_REV = os.environ.get('MOBILE_HEAD_REV')
HEAD_BRANCH = os.environ.get('MOBILE_HEAD_BRANCH')

BUILDER = lib.tasks.TaskBuilder(
    task_id=TASK_ID,
    owner="fenix-eng-notifications@mozilla.com",
    source='{}/raw/{}/.taskcluster.yml'.format(GITHUB_HTTP_REPOSITORY, HEAD_REV),
    scheduler_id=SCHEDULER_ID,
    build_worker_type=os.environ.get('BUILD_WORKER_TYPE'),
)


def generate_build_task(apks, is_staging):
    artifacts = {'public/{}'.format(os.path.basename(apk)): {
        "type": 'file',
        "path": apk,
        "expires": taskcluster.stringDate(taskcluster.fromNow('1 year')),
    } for apk in apks}

    checkout = (
        "export TERM=dumb && git fetch {} {} --tags && "
        "git config advice.detachedHead false && "
        "git checkout {}".format(
            GITHUB_HTTP_REPOSITORY, HEAD_BRANCH, HEAD_REV
        )
    )
    sentry_secret = '{}project/mobile/fenix/sentry'.format('garbage/staging/' if is_staging else '')
    leanplum_secret = '{}project/mobile/fenix/leanplum'.format('garbage/staging/' if is_staging else '')

    return taskcluster.slugId(), BUILDER.build_task(
        name="(Fenix) Build task",
        description="Build Fenix from source code.",
        command=(
            checkout +
            ' && python automation/taskcluster/helper/get-secret.py'
            ' -s {} -k dsn -f .sentry_token'.format(sentry_secret) +
            ' && python automation/taskcluster/helper/get-secret.py'
            ' -s {} -k production -f .leanplum_token'.format(leanplum_secret) +
            ' && ./gradlew --no-daemon -PcrashReports=true clean test assembleRelease'),
        features={
            "chainOfTrust": True,
            "taskclusterProxy": True
        },
        artifacts=artifacts,
        scopes=[
            "secrets:get:{}".format(sentry_secret),
            "secrets:get:{}".format(leanplum_secret)
        ]
    )


def generate_signing_task(build_task_id, apks, date, is_staging):
    artifacts = ["public/{}".format(os.path.basename(apk)) for apk in apks]

    signing_format = 'autograph_apk'
    index_release = 'staging-signed-nightly' if is_staging else 'signed-nightly'
    routes = [
        "index.project.mobile.fenix.{}.nightly.{}.{}.{}.latest".format(index_release, date.year, date.month, date.day),
        "index.project.mobile.fenix.{}.nightly.{}.{}.{}.revision.{}".format(index_release, date.year, date.month, date.day, HEAD_REV),
        "index.project.mobile.fenix.{}.nightly.latest".format(index_release),
    ]
    scopes = [
        "project:mobile:fenix:releng:signing:format:{}".format(signing_format),
        "project:mobile:fenix:releng:signing:cert:{}".format('dep-signing' if is_staging else 'release-signing')
    ]

    return taskcluster.slugId(), BUILDER.craft_signing_task(
        build_task_id,
        name="(Fenix) Signing task",
        description="Sign release builds of Fenix",
        apks=artifacts,
        scopes=scopes,
        routes=routes,
        signing_format=signing_format,
        is_staging=is_staging
    )


def generate_push_task(signing_task_id, apks, commit, is_staging):
    artifacts = ["public/{}".format(os.path.basename(apk)) for apk in apks]

    return taskcluster.slugId(), BUILDER.craft_push_task(
        signing_task_id,
        name="(Fenix) Push task",
        description="Upload signed release builds of Fenix to Google Play",
        apks=artifacts,
        scopes=[
            "project:mobile:fenix:releng:googleplay:product:fenix{}".format(':dep' if is_staging else '')
        ],
        commit=commit,
        is_staging=is_staging
    )


def populate_chain_of_trust_required_but_unused_files():
    # These files are needed to keep chainOfTrust happy. However, they have no need for Fenix
    # at the moment. For more details, see: https://github.com/mozilla-releng/scriptworker/pull/209/files#r184180585

    for file_name in ('actions.json', 'parameters.yml'):
        with open(file_name, 'w') as f:
            json.dump({}, f)


def nightly(apks, track, commit, date_string):
    queue = taskcluster.Queue({'baseUrl': 'http://taskcluster/queue/v1'})
    date = arrow.get(date_string)
    is_staging = track == 'staging-nightly'

    task_graph = {}

    build_task_id, build_task = generate_build_task(apks, is_staging)
    lib.tasks.schedule_task(queue, build_task_id, build_task)

    task_graph[build_task_id] = {}
    task_graph[build_task_id]['task'] = queue.task(build_task_id)

    sign_task_id, sign_task = generate_signing_task(build_task_id, apks, date, is_staging)
    lib.tasks.schedule_task(queue, sign_task_id, sign_task)

    task_graph[sign_task_id] = {}
    task_graph[sign_task_id]['task'] = queue.task(sign_task_id)

    push_task_id, push_task = generate_push_task(sign_task_id, apks, commit, is_staging)
    lib.tasks.schedule_task(queue, push_task_id, push_task)

    task_graph[push_task_id] = {}
    task_graph[push_task_id]['task'] = queue.task(push_task_id)

    print(json.dumps(task_graph, indent=4, separators=(',', ': ')))

    with open('task-graph.json', 'w') as f:
        json.dump(task_graph, f)

    populate_chain_of_trust_required_but_unused_files()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Create a release pipeline (build, sign, publish) on taskcluster.')

    parser.add_argument('--track', dest="track", action="store", choices=['nightly', 'staging-nightly'], required=True)
    parser.add_argument('--commit', action="store_true", help="commit the google play transaction")
    parser.add_argument('--apk', dest="apks", metavar="path", action="append", help="Path to APKs to sign and upload",
                        required=True)
    parser.add_argument('--output', metavar="path", action="store", help="Path to the build output",
                        required=True)
    parser.add_argument('--date', action="store", help="ISO8601 timestamp for build")

    result = parser.parse_args()
    apks = ["{}/{}".format(result.output, apk) for apk in result.apks]
    nightly(apks, result.track, result.commit, result.date)
