# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from voluptuous import Any, Required, Optional

from taskgraph.util.schema import taskref_or_string
from taskgraph.transforms.task import payload_builder


@payload_builder(
    "scriptworker-signing",
    schema={
        Required("max-run-time"): int,
        Required("signing-type"): str,
        # list of artifact URLs for the artifacts that should be signed
        Required("upstream-artifacts"): [
            {
                # taskId of the task with the artifact
                Required("taskId"): taskref_or_string,
                # type of signing task (for CoT)
                Required("taskType"): str,
                # Paths to the artifacts to sign
                Required("paths"): [str],
                # Signing formats to use on each of the paths
                Required("formats"): [str],
            }
        ],
    },
)
def build_scriptworker_signing_payload(config, task, task_def):
    worker = task["worker"]

    task_def["tags"]["worker-implementation"] = "scriptworker"

    task_def["payload"] = {
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
            f"{scope_prefix}:signing:format:{format}"
            for format in sorted(formats)
        ]
    )


@payload_builder(
    "scriptworker-beetmover",
    schema={
        Required("action"): str,
        Required("version"): str,
        Required("artifact-map"): [{
            Required("paths"): {
                Any(str): {
                    Required("destinations"): [str],
                },
            },
            Required("taskId"): taskref_or_string,
        }],
        Required("beetmover-application-name"): str,
        Required("bucket"): str,
        Required("upstream-artifacts"): [{
            Required("taskId"): taskref_or_string,
            Required("taskType"): str,
            Required("paths"): [str],
        }],
    },
)
def build_scriptworker_beetmover_payload(config, task, task_def):
    worker = task["worker"]

    task_def["tags"]["worker-implementation"] = "scriptworker"

    # Needed by beetmover-scriptworker
    for map_ in worker["artifact-map"]:
        map_["locale"] = "en-US"
        for path_config in map_["paths"].values():
            path_config["checksums_path"] = ""

    task_def["payload"] = {
        "artifactMap": worker["artifact-map"],
        "releaseProperties": {"appName": worker.pop("beetmover-application-name")},
        "upstreamArtifacts": worker["upstream-artifacts"],
        "version": worker["version"]
    }

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    task_def["scopes"].extend([
        "{}:beetmover:action:{}".format(scope_prefix, worker["action"]),
        "{}:beetmover:bucket:{}".format(scope_prefix, worker["bucket"]),
    ])


@payload_builder(
    "scriptworker-github",
    schema={
        Optional("upstream-artifacts"): [
            {
                Required("taskId"): taskref_or_string,
                Required("taskType"): str,
                Required("paths"): [str],
            }
        ],
        Required("artifact-map"): [object],
        Required("action"): str,
        Required("git-tag"): str,
        Required("git-revision"): str,
        Required("github-project"): str,
        Required("is-prerelease"): bool,
        Required("release-name"): str,
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
        "upstreamArtifacts": worker.get("upstream-artifacts", []),
    }

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    task_def["scopes"].extend([
        "{}:github:project:{}".format(scope_prefix, worker["github-project"]),
        "{}:github:action:{}".format(scope_prefix, worker["action"]),
    ])


@payload_builder(
    "scriptworker-shipit",
    schema={
        Required("release-name"): str,
    },
)
def build_shipit_payload(config, task, task_def):
    worker = task["worker"]

    task_def["tags"]["worker-implementation"] = "scriptworker"

    task_def['payload'] = {
        'release_name': worker['release-name']
    }


@payload_builder(
    "scriptworker-tree",
    schema={
        Optional("upstream-artifacts"): [
            {
                Optional("taskId"): taskref_or_string,
                Optional("taskType"): str,
                Optional("paths"): [str],
            }
        ],
        Required("bump"): bool,
        Optional("bump-files"): [str],
        Optional("push"): bool,
        Optional("branch"): str,
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
