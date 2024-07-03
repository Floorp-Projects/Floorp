# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

import logging
from typing import List

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_dependencies, get_primary_dependency
from taskgraph.util.schema import Schema
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.treeherder import inherit_treeherder_from_dep, replace_group
from voluptuous import Optional, Required

from gecko_taskgraph.transforms.beetmover import craft_release_properties
from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import (
    copy_attributes_from_dependent_job,
    sorted_unique_list,
)
from gecko_taskgraph.util.partials import (
    get_balrog_platform_name,
    get_partials_artifacts_from_params,
    get_partials_info_from_params,
)
from gecko_taskgraph.util.scriptworker import (
    generate_beetmover_artifact_map,
    generate_beetmover_partials_artifact_map,
    generate_beetmover_upstream_artifacts,
    get_beetmover_action_scope,
    get_beetmover_bucket_scope,
)

logger = logging.getLogger(__name__)


beetmover_description_schema = Schema(
    {
        # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
        Required("label"): str,
        Required("dependencies"): task_description_schema["dependencies"],
        # treeherder is allowed here to override any defaults we use for beetmover.  See
        # taskcluster/gecko_taskgraph/transforms/task.py for the schema details, and the
        # below transforms for defaults of various values.
        Optional("treeherder"): task_description_schema["treeherder"],
        Optional("attributes"): task_description_schema["attributes"],
        # locale is passed only for l10n beetmoving
        Optional("locale"): str,
        Required("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("task-from"): task_description_schema["task-from"],
    }
)

transforms = TransformSequence()


@transforms.add
def remove_name(config, jobs):
    for job in jobs:
        if "name" in job:
            del job["name"]
        yield job


transforms.add_validate(beetmover_description_schema)


def get_label_by_suffix(labels: List, suffix: str):
    """
    Given list of labels, returns the label with provided suffix
    Raises exception if more than one label is found.

    Args:
        labels (List): List of labels
        suffix (str): Suffix for the desired label

    Returns
        str: The desired label
    """
    labels = [l for l in labels if l.endswith(suffix)]
    if len(labels) > 1:
        raise Exception(
            f"There should only be a single label with suffix: {suffix} - found {len(labels)}"
        )
    return labels[0]


@transforms.add
def gather_required_signoffs(config, jobs):
    for job in jobs:
        job.setdefault("attributes", {})["required_signoffs"] = sorted_unique_list(
            *(
                dep.attributes.get("required_signoffs", [])
                for dep in get_dependencies(config, job)
            )
        )
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = dep_job.attributes

        treeherder = inherit_treeherder_from_dep(job, dep_job)
        upstream_symbol = dep_job.task["extra"]["treeherder"]["symbol"]
        if "build" in job["dependencies"]:
            build_label = job["dependencies"]["build"]
            build_dep = config.kind_dependencies_tasks[build_label]
            upstream_symbol = build_dep.task["extra"]["treeherder"]["symbol"]
        treeherder.setdefault("symbol", replace_group(upstream_symbol, "BMR"))
        label = job["label"]
        description = (
            "Beetmover submission for locale '{locale}' for build '"
            "{build_platform}/{build_type}'".format(
                locale=attributes.get("locale", "en-US"),
                build_platform=attributes.get("build_platform"),
                build_type=attributes.get("build_type"),
            )
        )

        upstream_deps = {
            k: config.kind_dependencies_tasks[v] for k, v in job["dependencies"].items()
        }

        signing_name = "build-signing"
        build_name = "build"
        repackage_name = "repackage"
        repackage_signing_name = "repackage-signing"
        msi_signing_name = "repackage-signing-msi"
        msix_signing_name = "repackage-signing-shippable-l10n-msix"
        mar_signing_name = "mar-signing"
        attribution_name = "attribution"
        repackage_deb_name = "repackage-deb"
        if job.get("locale"):
            signing_name = "shippable-l10n-signing"
            build_name = "shippable-l10n"
            repackage_name = "repackage-l10n"
            repackage_signing_name = "repackage-signing-l10n"
            mar_signing_name = "mar-signing-l10n"
            attribution_name = "attribution-l10n"
            repackage_deb_name = "repackage-deb-l10n"

        # The upstream "signing" task for macosx is either *-mac-signing or *-mac-notarization
        if attributes.get("build_platform", "").startswith("macosx"):
            signing_name = None
            # We use the signing task on level 1 and notarization on level 3
            if int(config.params.get("level", 0)) < 3:
                signing_name = get_label_by_suffix(job["dependencies"], "-mac-signing")
            else:
                signing_name = get_label_by_suffix(
                    job["dependencies"], "-mac-notarization"
                )
            if not signing_name:
                raise Exception("Could not find upstream kind for mac signing.")

        dependencies = {
            "build": upstream_deps[build_name],
            "repackage": upstream_deps[repackage_name],
            "signing": upstream_deps[signing_name],
            "mar-signing": upstream_deps[mar_signing_name],
        }
        if "partials-signing" in upstream_deps:
            dependencies["partials-signing"] = upstream_deps["partials-signing"]
        if msi_signing_name in upstream_deps:
            dependencies[msi_signing_name] = upstream_deps[msi_signing_name]
        if msix_signing_name in upstream_deps:
            dependencies[msix_signing_name] = upstream_deps[msix_signing_name]
        if repackage_signing_name in upstream_deps:
            dependencies["repackage-signing"] = upstream_deps[repackage_signing_name]
        if attribution_name in upstream_deps:
            dependencies[attribution_name] = upstream_deps[attribution_name]
        if repackage_deb_name in upstream_deps:
            dependencies[repackage_deb_name] = upstream_deps[repackage_deb_name]

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get("attributes", {}))
        if job.get("locale"):
            attributes["locale"] = job["locale"]

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)

        task = {
            "label": label,
            "description": description,
            "worker-type": "beetmover",
            "scopes": [bucket_scope, action_scope],
            "dependencies": dependencies,
            "attributes": attributes,
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "treeherder": treeherder,
            "shipping-phase": job["shipping-phase"],
            "shipping-product": job.get("shipping-product"),
        }

        yield task


def generate_partials_upstream_artifacts(job, artifacts, platform, locale=None):
    artifact_prefix = get_artifact_prefix(job)
    if locale and locale != "en-US":
        artifact_prefix = f"{artifact_prefix}/{locale}"

    upstream_artifacts = [
        {
            "taskId": {"task-reference": "<partials-signing>"},
            "taskType": "signing",
            "paths": [f"{artifact_prefix}/{path}" for path, _ in artifacts],
            "locale": locale or "en-US",
        }
    ]

    return upstream_artifacts


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]

        worker = {
            "implementation": "beetmover",
            "release-properties": craft_release_properties(config, job),
            "upstream-artifacts": generate_beetmover_upstream_artifacts(
                config, job, platform, locale
            ),
            "artifact-map": generate_beetmover_artifact_map(
                config, job, platform=platform, locale=locale
            ),
        }

        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job


@transforms.add
def strip_unwanted_langpacks_from_worker(config, jobs):
    """Strips out langpacks where we didn't sign them.

    This explicitly deletes langpacks from upstream artifacts and from artifact-maps.
    Due to limitations in declarative artifacts, doing this was our easiest way right now.
    """
    ALWAYS_OK_PLATFORMS = {"linux64-shippable", "linux64-devedition"}
    OSX_OK_PLATFORMS = {"macosx64-shippable", "macosx64-devedition"}
    for job in jobs:
        platform = job["attributes"].get("build_platform")
        if platform in ALWAYS_OK_PLATFORMS:
            # No need to strip anything
            yield job
            continue

        for map in job["worker"].get("artifact-map", [])[:]:
            if not any([path.endswith("target.langpack.xpi") for path in map["paths"]]):
                continue
            if map["locale"] == "ja-JP-mac":
                # This locale should only exist on mac
                assert platform in OSX_OK_PLATFORMS
                continue
            # map[paths] is being modified while iterating, so we need to resolve the
            # ".keys()" iterator up front by throwing it into a list.
            for path in list(map["paths"].keys()):
                if path.endswith("target.langpack.xpi"):
                    del map["paths"][path]
            if map["paths"] == {}:
                job["worker"]["artifact-map"].remove(map)

        for artifact in job["worker"].get("upstream-artifacts", []):
            if not any(
                [path.endswith("target.langpack.xpi") for path in artifact["paths"]]
            ):
                continue
            if artifact["locale"] == "ja-JP-mac":
                # This locale should only exist on mac
                assert platform in OSX_OK_PLATFORMS
                continue
            artifact["paths"] = [
                path
                for path in artifact["paths"]
                if not path.endswith("target.langpack.xpi")
            ]
            if artifact["paths"] == []:
                job["worker"]["upstream-artifacts"].remove(artifact)

        yield job


@transforms.add
def make_partials_artifacts(config, jobs):
    for job in jobs:
        locale = job["attributes"].get("locale")
        if not locale:
            locale = "en-US"

        platform = job["attributes"]["build_platform"]

        if "partials-signing" not in job["dependencies"]:
            yield job
            continue

        balrog_platform = get_balrog_platform_name(platform)
        artifacts = get_partials_artifacts_from_params(
            config.params.get("release_history"), balrog_platform, locale
        )

        upstream_artifacts = generate_partials_upstream_artifacts(
            job, artifacts, balrog_platform, locale
        )

        job["worker"]["upstream-artifacts"].extend(upstream_artifacts)

        extra = list()

        partials_info = get_partials_info_from_params(
            config.params.get("release_history"), balrog_platform, locale
        )

        job["worker"]["artifact-map"].extend(
            generate_beetmover_partials_artifact_map(
                config, job, partials_info, platform=platform, locale=locale
            )
        )

        for artifact in partials_info:
            artifact_extra = {
                "locale": locale,
                "artifact_name": artifact,
                "buildid": partials_info[artifact]["buildid"],
                "platform": balrog_platform,
            }
            for rel_attr in ("previousBuildNumber", "previousVersion"):
                if partials_info[artifact].get(rel_attr):
                    artifact_extra[rel_attr] = partials_info[artifact][rel_attr]
            extra.append(artifact_extra)

        job.setdefault("extra", {})
        job["extra"]["partials"] = extra

        yield job


@transforms.add
def convert_deps(config, jobs):
    for job in jobs:
        job["dependencies"] = {
            name: dep_job.label for name, dep_job in job["dependencies"].items()
        }
        yield job
