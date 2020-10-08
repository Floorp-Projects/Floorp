# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from six import text_type

from voluptuous import Any, Required, Optional

from taskgraph.util.schema import taskref_or_string, WHITELISTED_SCHEMA_IDENTIFIERS
from taskgraph.transforms.task import payload_builder

WHITELISTED_SCHEMA_IDENTIFIERS.extend([
    lambda path: "[u'artifact-map']" in path,   # handed directly in beetmover
])


@payload_builder(
    "scriptworker-signing",
    schema={
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
    "scriptworker-beetmover",
    schema={
        Required("action"): text_type,
        Required("version"): text_type,
        Required("artifact-map"): [{
            Required("paths"): {
                Any(text_type): {
                    Required("destinations"): [text_type],
                },
            },
            Required("taskId"): taskref_or_string,
        }],
        Required("beetmover-application-name"): text_type,
        Required("bucket"): text_type,
        Required("upstream-artifacts"): [{
            Required("taskId"): taskref_or_string,
            Required("taskType"): text_type,
            Required("paths"): [text_type],
        }],
    },
)
def build_scriptworker_signing_payload(config, task, task_def):
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
    }

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    task_def["scopes"].extend([
        "{}:beetmover:action:{}".format(scope_prefix, worker["action"]),
        "{}:beetmover:bucket:{}".format(scope_prefix, worker["bucket"]),
    ])
