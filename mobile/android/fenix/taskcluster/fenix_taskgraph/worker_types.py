# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from six import text_type

from voluptuous import Any, Required, Optional

from taskgraph.util.schema import taskref_or_string
from taskgraph.transforms.task import payload_builder


@payload_builder(
    "scriptworker-signing",
    schema={
        # the maximum time to run, in seconds
        Required("max-run-time"): int,
        Required("signing-type"): text_type,
        # list of artifact URLs for the artifacts that should be signed
        Required("upstream-artifacts"): [
            {
                # taskId of the task with the artifact
                Required("taskId"): taskref_or_string,
                # type of signing task (for CoT)
                Required("taskType"): text_type,
                # Paths to the artifacts to sign
                Required("paths"): [text_type],
                # Signing formats to use on each of the paths
                Required("formats"): [text_type],
            }
        ],
    },
)
def build_scriptworker_signing_payload(config, task, task_def):
    worker = task["worker"]

    task_def["tags"]["worker-implementation"] = "scriptworker"

    task_def["payload"] = {
        "maxRunTime": worker["max-run-time"],
        "upstreamArtifacts": worker["upstream-artifacts"],
    }

    formats = set()
    for artifacts in worker["upstream-artifacts"]:
        formats.update(artifacts["formats"])

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    task_def["scopes"].append(
        "{}:signing:cert:{}".format(scope_prefix, worker["signing-type"])
    )
    task_def["scopes"].extend(
        [
            "{}:signing:format:{}".format(scope_prefix, format)
            for format in sorted(formats)
        ]
    )


@payload_builder(
    "scriptworker-pushapk",
    schema={
        Required("upstream-artifacts"): [
            {
                Required("taskId"): taskref_or_string,
                Required("taskType"): text_type,
                Required("paths"): [text_type],
            }
        ],
        Required("certificate-alias"): text_type,
        Required("channel"): text_type,
        Required("commit"): bool,
        Required("product"): text_type,
        Required("dep"): bool,
        Optional("google-play-track"): text_type,
    },
)
def build_push_apk_payload(config, task, task_def):
    worker = task["worker"]

    task_def["tags"]["worker-implementation"] = "scriptworker"

    task_def["payload"] = {
        "certificate_alias": worker["certificate-alias"],
        "channel": worker["channel"],
        "commit": worker["commit"],
        "upstreamArtifacts": worker["upstream-artifacts"],
    }
    if worker.get("google-play-track"):
        task_def["payload"]["google_play_track"] = worker["google-play-track"]

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    task_def["scopes"].append(
        "{}:googleplay:product:{}{}".format(
            scope_prefix, worker["product"], ":dep" if worker["dep"] else ""
        )
    )


@payload_builder(
    "scriptworker-shipit",
    schema={
        Required("upstream-artifacts"): [
            {
                Required("taskId"): taskref_or_string,
                Required("taskType"): text_type,
                Required("paths"): [text_type],
            }
        ],
        Required("release-name"): text_type,
    },
)
def build_shipit_payload(config, task, task_def):
    worker = task["worker"]

    task_def["tags"]["worker-implementation"] = "scriptworker"

    task_def['payload'] = {
        'release_name': worker['release-name']
    }


@payload_builder(
    "scriptworker-github",
    schema={
        Required("upstream-artifacts"): [
            {
                Required("taskId"): taskref_or_string,
                Required("taskType"): text_type,
                Required("paths"): [text_type],
            }
        ],
        Required("artifact-map"): [object],
        Required("action"): text_type,
        Required("git-tag"): text_type,
        Required("git-revision"): text_type,
        Required("github-project"): text_type,
        Required("is-prerelease"): bool,
        Required("release-name"): text_type,
    },
)
def build_github_release_payload(config, task, task_def):
    worker = task["worker"]

    task_def["tags"]["worker-implementation"] = "scriptworker"

    task_def["payload"] = {
        "artifactMap": worker["artifact-map"],
        "gitTag": worker["git-tag"],
        "gitRevision": worker["git-revision"],
        "isPrerelease": worker["is-prerelease"],
        "releaseName": worker["release-name"],
        "upstreamArtifacts": worker["upstream-artifacts"],
    }

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    task_def["scopes"].extend([
        "{}:github:project:{}".format(scope_prefix, worker["github-project"]),
        "{}:github:action:{}".format(scope_prefix, worker["action"]),
    ])


@payload_builder(
    "scriptworker-tree",
    schema={
        Optional("upstream-artifacts"): [
            {
                Optional("taskId"): taskref_or_string,
                Optional("taskType"): text_type,
                Optional("paths"): [text_type],
            }
        ],
        Required("bump"): bool,
        Optional("bump-files"): [text_type],
        Optional("push"): bool,
        Optional("branch"): text_type,
    },
)
def build_version_bump_payload(config, task, task_def):
    worker = task["worker"]
    task_def["tags"]["worker-implementation"] = "scriptworker"

    task_def['payload'] = {'actions': []}
    actions = task_def['payload']['actions']

    if worker['bump']:
        if not worker['bump-files']:
            raise Exception("Version Bump requested without bump-files")

        bump_info = {}
        bump_info["next_version"] = config.params["next_version"]
        bump_info['files'] = worker['bump-files']
        task_def['payload']['version_bump_info'] = bump_info
        actions.append('version_bump')

    if worker["push"]:
        task_def['payload']['push'] = True

    if worker.get('force-dry-run'):
        task_def['payload']['dry_run'] = True

    if worker.get("branch"):
        task_def["payload"]["branch"] = worker["branch"]
