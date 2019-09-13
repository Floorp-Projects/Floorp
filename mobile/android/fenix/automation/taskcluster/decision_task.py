# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Decision task for nightly releases.
"""

from __future__ import print_function

import argparse
import datetime
import os
import re

import taskcluster

from lib.gradle import get_variant
from lib.tasks import (
    fetch_mozharness_task_id,
    schedule_task_graph,
    TaskBuilder,
)
from lib.chain_of_trust import (
    populate_chain_of_trust_task_graph,
    populate_chain_of_trust_required_but_unused_files
)

def pr(builder):
    tasks = []

    variant = get_variant('debug', 'geckoNightly')
    tasks.append(builder.craft_assemble_pr_task(variant))
    tasks.append(builder.craft_test_pr_task(variant))

    for craft_function in (
        builder.craft_detekt_task,
        builder.craft_ktlint_task,
        builder.craft_lint_task,
        builder.craft_compare_locales_task,
    ):
        tasks.append(craft_function())

    return tasks


def push(builder):
    all_tasks = pr(builder)
    all_tasks.append(builder.craft_ui_tests_task())
    return all_tasks


def raptor(builder, is_staging):
    mozharness_task_id = fetch_mozharness_task_id()
    gecko_revision = taskcluster.Queue({
      'rootUrl': os.environ.get('TASKCLUSTER_PROXY_URL', 'https://taskcluster.net'),
    }).task(mozharness_task_id)['payload']['env']['GECKO_HEAD_REV']

    variant = get_variant('forPerformanceTest', 'geckoNightly')
    build_task = builder.craft_assemble_raptor_task(variant)
    signing_task = builder.craft_raptor_signing_task(build_task['label'], variant, is_staging)

    tasks = [build_task, signing_task]

    for abi in ('armeabi-v7a', 'arm64-v8a'):
        variant_apk = variant.get_apk(abi)
        all_raptor_craft_functions = [
            builder.craft_raptor_tp6m_cold_task(for_suite=i)
                for i in range(1, 28)
            ] + [
                builder.craft_raptor_youtube_playback_task,
            ]
        for craft_function in all_raptor_craft_functions:
            raptor_task = craft_function(
                signing_task['label'], mozharness_task_id, variant_apk, gecko_revision, is_staging
            )
            tasks.append(raptor_task)

    return tasks


def release(builder, channel, engine, is_staging, version_name):
    variant = get_variant('fenix' + channel.capitalize(), engine)
    taskcluster_apk_paths = variant.upstream_artifacts()

    build_task = builder.craft_assemble_release_task(variant, channel, is_staging, version_name)

    signing_task = builder.craft_release_signing_task(
        build_task['label'],
        taskcluster_apk_paths,
        channel=channel,
        is_staging=is_staging,
    )

    push_task = builder.craft_push_task(
        signing_task['label'],
        taskcluster_apk_paths,
        channel=channel,
        # TODO until org.mozilla.fenix.nightly is made public, put it on the internally-testable track
        override_google_play_track=None if channel != "nightly" else "internal",
        is_staging=is_staging,
    )

    return [build_task, signing_task, push_task]


def release_as_fennec(builder, is_staging, version_name):
    variant = get_variant('fennecProduction', 'geckoBeta')
    channel = 'fennec-production'

    build_tasks = {}
    signing_tasks = {}

    build_task_id = _generate_slug_id()
    build_tasks[build_task_id] = builder.craft_assemble_release_task(variant, channel, is_staging, version_name)

    signing_task_id = _generate_slug_id()
    signing_tasks[signing_task_id] = builder.craft_release_signing_task(
        build_task_id,
        variant.upstream_artifacts(),
        channel,
        is_staging,
    )

    return (build_tasks, signing_tasks)


def nightly_to_production_app(builder, is_staging, version_name):
    # Since the Fenix nightly was launched, we've pushed it to the production app "org.mozilla.fenix" on the
    # "nightly" track. We're moving towards having each channel be published to its own app, but we need to
    # keep updating this "backwards-compatible" nightly for a while yet
    variant = get_variant('fenixNightlyLegacy', 'geckoNightly')
    taskcluster_apk_paths = variant.upstream_artifacts()

    build_tasks = {}
    signing_tasks = {}
    push_tasks = {}
    other_tasks = {}

    build_task_id = _generate_slug_id()
    build_tasks[build_task_id] = builder.craft_assemble_release_task(
        variant, 'nightly-legacy', is_staging, version_name)

    signing_task_id = _generate_slug_id()
    signing_tasks[signing_task_id] = builder.craft_release_signing_task(
        build_task_id,
        taskcluster_apk_paths,
        channel='production',  # Since we're publishing to the "production" app, we need to sign for production
        is_staging=is_staging,
        publish_to_index=False,
    )

    push_task_id = _generate_slug_id()
    push_tasks[push_task_id] = builder.craft_push_task(
        signing_task_id,
        taskcluster_apk_paths,
        channel='production',  # We're publishing to the "production" app on the "nightly" track
        override_google_play_track='nightly',
        is_staging=is_staging,
    )

    if not is_staging:
        nimbledroid_task_id = _generate_slug_id()
        other_tasks[nimbledroid_task_id] = builder.craft_upload_apk_nimbledroid_task(
            build_task_id
        )

    return (build_tasks, signing_tasks, push_tasks, other_tasks)


def _generate_slug_id():
    return taskcluster.slugId()
