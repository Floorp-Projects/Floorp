# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

import copy
import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.schema import Schema, optionally_keyed_by, resolve_keyed_by
from taskgraph.util.treeherder import inherit_treeherder_from_dep
from voluptuous import Optional, Required

from gecko_taskgraph.transforms.beetmover import craft_release_properties
from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import (
    copy_attributes_from_dependent_job,
    release_level,
)
from gecko_taskgraph.util.scriptworker import (
    generate_beetmover_artifact_map,
    generate_beetmover_upstream_artifacts,
    get_beetmover_action_scope,
    get_beetmover_bucket_scope,
)

logger = logging.getLogger(__name__)


transforms = TransformSequence()


beetmover_description_schema = Schema(
    {
        # attributes is used for enabling artifact-map by declarative artifacts
        Required("attributes"): {str: object},
        # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
        Optional("label"): str,
        # treeherder is allowed here to override any defaults we use for beetmover.  See
        # taskcluster/gecko_taskgraph/transforms/task.py for the schema details, and the
        # below transforms for defaults of various values.
        Optional("treeherder"): task_description_schema["treeherder"],
        Required("description"): str,
        Required("worker-type"): optionally_keyed_by("release-level", str),
        Required("run-on-projects"): [],
        # locale is passed only for l10n beetmoving
        Optional("locale"): str,
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("job-from"): task_description_schema["job-from"],
        Optional("dependencies"): task_description_schema["dependencies"],
    }
)


@transforms.add
def remove_name(config, jobs):
    for job in jobs:
        if "name" in job:
            del job["name"]
        yield job


transforms.add_validate(beetmover_description_schema)


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        for field in ("worker-type", "attributes.artifact_map"):
            resolve_keyed_by(
                job,
                field,
                item_name=job["label"],
                **{
                    "release-level": release_level(config.params["project"]),
                    "release-type": config.params["release_type"],
                    "project": config.params["project"],
                },
            )
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = dep_job.attributes

        treeherder = inherit_treeherder_from_dep(job, dep_job)
        treeherder.setdefault(
            "symbol", "langpack(BM{})".format(attributes.get("l10n_chunk", ""))
        )

        job["attributes"].update(copy_attributes_from_dependent_job(dep_job))
        job["attributes"]["chunk_locales"] = dep_job.attributes.get(
            "chunk_locales", ["en-US"]
        )

        job["description"] = job["description"].format(
            locales="/".join(job["attributes"]["chunk_locales"]),
            platform=job["attributes"]["build_platform"],
        )

        job["scopes"] = [
            get_beetmover_bucket_scope(config),
            get_beetmover_action_scope(config),
        ]

        job["dependencies"] = {"langpack-copy": dep_job.label}

        job["run-on-projects"] = job.get(
            "run_on_projects", dep_job.attributes["run_on_projects"]
        )
        job["treeherder"] = treeherder
        job["shipping-phase"] = job.get(
            "shipping-phase", dep_job.attributes["shipping_phase"]
        )
        job["shipping-product"] = dep_job.attributes["shipping_product"]

        yield job


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        platform = job["attributes"]["build_platform"]
        locale = job["attributes"]["chunk_locales"]

        job["worker"] = {
            "implementation": "beetmover",
            "release-properties": craft_release_properties(config, job),
            "upstream-artifacts": generate_beetmover_upstream_artifacts(
                config,
                job,
                platform,
                locale,
            ),
            "artifact-map": generate_beetmover_artifact_map(
                config, job, platform=platform, locale=locale
            ),
        }

        yield job


@transforms.add
def yield_all_platform_jobs(config, jobs):
    # Even though langpacks are now platform independent, we keep beetmoving them at old
    # platform-specific locations. That's why this transform exist
    # The linux64 and mac specific ja-JP-mac are beetmoved along with the signing beetmover
    # So while the dependent jobs are linux here, we only yield jobs for other platforms
    for job in jobs:
        platforms = ("linux", "macosx64", "win32", "win64")
        if "devedition" in job["attributes"]["build_platform"]:
            platforms = (f"{plat}-devedition" for plat in platforms)
        for platform in platforms:
            platform_job = copy.deepcopy(job)
            if "ja" in platform_job["attributes"]["chunk_locales"] and platform in (
                "macosx64",
                "macosx64-devedition",
            ):
                platform_job = _strip_ja_data_from_linux_job(platform_job)

            platform_job = _change_platform_data(config, platform_job, platform)

            yield platform_job


def _strip_ja_data_from_linux_job(platform_job):
    # Let's take "ja" out the description. This locale is in a substring like "aa/bb/cc/dd", where
    # "ja" could be any of "aa", "bb", "cc", "dd"
    platform_job["description"] = platform_job["description"].replace("ja/", "")
    platform_job["description"] = platform_job["description"].replace("/ja", "")

    platform_job["worker"]["upstream-artifacts"] = [
        artifact
        for artifact in platform_job["worker"]["upstream-artifacts"]
        if artifact["locale"] != "ja"
    ]

    return platform_job


def _change_platform_in_artifact_map_paths(paths, orig_platform, new_platform):
    amended_paths = {}
    for artifact, artifact_info in paths.items():
        amended_artifact_info = {
            "checksums_path": artifact_info["checksums_path"].replace(
                orig_platform, new_platform
            ),
            "destinations": [
                d.replace(orig_platform, new_platform)
                for d in artifact_info["destinations"]
            ],
        }
        amended_paths[artifact] = amended_artifact_info

    return amended_paths


def _change_platform_data(config, platform_job, platform):
    orig_platform = "linux64"
    if "devedition" in platform:
        orig_platform = "linux64-devedition"
    platform_job["attributes"]["build_platform"] = platform
    platform_job["label"] = platform_job["label"].replace(orig_platform, platform)
    platform_job["description"] = platform_job["description"].replace(
        orig_platform, platform
    )
    platform_job["treeherder"]["platform"] = platform_job["treeherder"][
        "platform"
    ].replace(orig_platform, platform)
    platform_job["worker"]["release-properties"]["platform"] = platform

    # amend artifactMap entries as well
    platform_mapping = {
        "linux64": "linux-x86_64",
        "linux": "linux-i686",
        "macosx64": "mac",
        "win32": "win32",
        "win64": "win64",
        "linux64-devedition": "linux-x86_64",
        "linux-devedition": "linux-i686",
        "macosx64-devedition": "mac",
        "win32-devedition": "win32",
        "win64-devedition": "win64",
    }
    orig_platform = platform_mapping.get(orig_platform, orig_platform)
    platform = platform_mapping.get(platform, platform)
    platform_job["worker"]["artifact-map"] = [
        {
            "locale": entry["locale"],
            "taskId": entry["taskId"],
            "paths": _change_platform_in_artifact_map_paths(
                entry["paths"], orig_platform, platform
            ),
        }
        for entry in platform_job["worker"]["artifact-map"]
    ]

    return platform_job
