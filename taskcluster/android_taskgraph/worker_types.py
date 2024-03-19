# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from gecko_taskgraph.transforms.task import payload_builder
from taskgraph.util.schema import taskref_or_string
from voluptuous import Any, Optional, Required


@payload_builder(
    "scriptworker-beetmover",
    schema={
        Required("action"): str,
        Required("version"): str,
        Required("artifact-map"): [
            {
                Required("paths"): {
                    Any(str): {
                        Required("destinations"): [str],
                    },
                },
                Required("taskId"): taskref_or_string,
            }
        ],
        Required("beetmover-application-name"): str,
        Required("bucket"): str,
        Required("upstream-artifacts"): [
            {
                Required("taskId"): taskref_or_string,
                Required("taskType"): str,
                Required("paths"): [str],
            }
        ],
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
        "version": worker["version"],
    }

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    task_def["scopes"].extend(
        [
            "{}:beetmover:action:{}".format(scope_prefix, worker["action"]),
            "{}:beetmover:bucket:{}".format(scope_prefix, worker["bucket"]),
        ]
    )


@payload_builder(
    "scriptworker-pushapk",
    schema={
        Required("upstream-artifacts"): [
            {
                Required("taskId"): taskref_or_string,
                Required("taskType"): str,
                Required("paths"): [str],
            }
        ],
        Required("certificate-alias"): str,
        Required("channel"): str,
        Required("commit"): bool,
        Required("product"): str,
        Required("dep"): bool,
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

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    task_def["scopes"].append(
        "{}:googleplay:product:{}{}".format(
            scope_prefix, worker["product"], ":dep" if worker["dep"] else ""
        )
    )


@payload_builder(
    "scriptworker-shipit",
    schema={
        Required("release-name"): str,
    },
)
def build_shipit_payload(config, task, task_def):
    worker = task["worker"]

    task_def["tags"]["worker-implementation"] = "scriptworker"

    task_def["payload"] = {"release_name": worker["release-name"]}


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

    scopes = task_def.setdefault("scopes", [])
    scope_prefix = f"project:mobile:{config.params['project']}:treescript:action"
    task_def["payload"] = {}

    if worker["bump"]:
        if not worker["bump-files"]:
            raise Exception("Version Bump requested without bump-files")

        bump_info = {}
        bump_info["next_version"] = config.params["next_version"]
        bump_info["files"] = worker["bump-files"]
        task_def["payload"]["version_bump_info"] = bump_info
        scopes.append(f"{scope_prefix}:version_bump")

    if worker["push"]:
        task_def["payload"]["push"] = True

    if worker.get("force-dry-run"):
        task_def["payload"]["dry_run"] = True

    if worker.get("branch"):
        task_def["payload"]["branch"] = worker["branch"]
