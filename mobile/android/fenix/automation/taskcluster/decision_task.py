# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Decision task for nightly releases.
"""

from __future__ import print_function

import argparse
import os
import sys
import taskcluster

from lib import build_variants
from lib.tasks import TaskBuilder, schedule_task_graph
from lib.util import (
    populate_chain_of_trust_task_graph,
    populate_chain_of_trust_required_but_unused_files
)

REPO_URL = os.environ.get('MOBILE_HEAD_REPOSITORY')
COMMIT = os.environ.get('MOBILE_HEAD_REV')
PR_TITLE = os.environ.get('GITHUB_PULL_TITLE', '')

# If we see this text inside a pull request title then we will not execute any tasks for this PR.
SKIP_TASKS_TRIGGER = '[ci skip]'


BUILDER = TaskBuilder(
    task_id=os.environ.get('TASK_ID'),
    repo_url=os.environ.get('MOBILE_HEAD_REPOSITORY'),
    branch=os.environ.get('MOBILE_HEAD_BRANCH'),
    commit=COMMIT,
    owner="fenix-eng-notifications@mozilla.com",
    source='{}/raw/{}/.taskcluster.yml'.format(REPO_URL, COMMIT),
    scheduler_id=os.environ.get('SCHEDULER_ID', 'taskcluster-github'),
    tasks_priority=os.environ.get('TASKS_PRIORITY'),
)


def pr_or_push():
    if SKIP_TASKS_TRIGGER in PR_TITLE:
        print("Pull request title contains", SKIP_TASKS_TRIGGER)
        print("Exit")
        exit(0)

    print("Fetching build variants from gradle")
    variants = build_variants.from_gradle()

    if len(variants) == 0:
        print("Could not get build variants from gradle")
        sys.exit(2)

    print("Got variants: {}".format(' '.join(variants)))

    build_tasks = {}
    other_tasks = {}

    for variant in variants:
        build_tasks[taskcluster.slugId()] = BUILDER.craft_assemble_task(variant)
        build_tasks[taskcluster.slugId()] = BUILDER.craft_test_task(variant)

    for craft_function in (
        BUILDER.craft_detekt_task,
        BUILDER.craft_ktlint_task,
        BUILDER.craft_lint_task,
        BUILDER.craft_compare_locales_task,
    ):
        other_tasks[taskcluster.slugId()] = craft_function()

    return (build_tasks, other_tasks)


def nightly(apks, track, commit, date_string):
    is_staging = track == 'staging-nightly'

    build_tasks = {}
    signing_tasks = {}
    push_tasks = {}
    artifacts = ["public/{}".format(os.path.basename(apk)) for apk in apks]

    build_task_id = taskcluster.slugId()
    build_tasks[build_task_id] = BUILDER.craft_assemble_release_task(apks, is_staging)

    signing_task_id = taskcluster.slugId()
    signing_tasks[signing_task_id] = BUILDER.craft_signing_task(
        build_task_id,
        apks=artifacts,
        date_string=date_string,
        is_staging=is_staging,
    )

    push_task_id = taskcluster.slugId()
    push_tasks[push_task_id] = BUILDER.craft_push_task(
        signing_task_id,
        apks=artifacts,
        commit=commit,
        is_staging=is_staging
    )

    return (build_tasks, signing_tasks, push_tasks)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Creates and submit a graph of tasks on Taskcluster.'
    )

    subparsers = parser.add_subparsers(dest='command')

    subparsers.add_parser('pr-or-push')
    release_parser = subparsers.add_parser('release')

    release_parser.add_argument('--nightly', action="store_true", default=False)
    release_parser.add_argument(
        '--track', action="store", choices=['nightly', 'staging-nightly'], required=True
    )
    release_parser.add_argument(
        '--commit', action="store_true", help="commit the google play transaction"
    )
    release_parser.add_argument(
        '--apk', dest="apks", metavar="path", action="append",
        help="Path to APKs to sign and upload", required=True
    )
    release_parser.add_argument(
        '--output', metavar="path", action="store", help="Path to the build output", required=True
    )
    release_parser.add_argument('--date', action="store", help="ISO8601 timestamp for build")

    result = parser.parse_args()

    command = result.command

    if command == 'pr-or-push':
        ordered_groups_of_tasks = pr_or_push()
    elif command == 'release':
        apks = ["{}/{}".format(result.output, apk) for apk in result.apks]
        # nightly(apks, result.track, result.commit, result.date)
        ordered_groups_of_tasks = nightly(
            apks, result.track, result.commit, result.date
        )
    else:
        raise Exception('Unsupported command "{}"'.format(command))

    full_task_graph = schedule_task_graph(ordered_groups_of_tasks)

    populate_chain_of_trust_task_graph(full_task_graph)
    populate_chain_of_trust_required_but_unused_files()
