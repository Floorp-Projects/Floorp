# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-sign-and-push task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.schema import Schema, optionally_keyed_by, resolve_keyed_by
from taskgraph.util.treeherder import inherit_treeherder_from_dep
from voluptuous import Any, Optional, Required

from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import (
    copy_attributes_from_dependent_job,
    release_level,
)

transforms = TransformSequence()

langpack_sign_push_description_schema = Schema(
    {
        Required("label"): str,
        Required("description"): str,
        Required("worker-type"): optionally_keyed_by("release-level", str),
        Required("worker"): {
            Required("channel"): optionally_keyed_by(
                "project", "platform", Any("listed", "unlisted")
            ),
            Required("upstream-artifacts"): None,  # Processed here below
        },
        Required("run-on-projects"): [],
        Required("scopes"): optionally_keyed_by("release-level", [str]),
        Required("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("job-from"): task_description_schema["job-from"],
        Optional("attributes"): task_description_schema["attributes"],
        Optional("dependencies"): task_description_schema["dependencies"],
    }
)


@transforms.add
def set_label(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        job["label"] = f"push-langpacks-{dep_job.label}"

        if "name" in job:
            del job["name"]

        yield job


transforms.add_validate(langpack_sign_push_description_schema)


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        resolve_keyed_by(
            job,
            "worker-type",
            item_name=job["label"],
            **{"release-level": release_level(config.params["project"])},
        )
        resolve_keyed_by(
            job,
            "scopes",
            item_name=job["label"],
            **{"release-level": release_level(config.params["project"])},
        )
        resolve_keyed_by(
            job,
            "worker.channel",
            item_name=job["label"],
            project=config.params["project"],
            platform=dep_job.attributes["build_platform"],
        )

        yield job


@transforms.add
def copy_attributes(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        job.setdefault("attributes", {}).update(
            copy_attributes_from_dependent_job(dep_job)
        )
        job["attributes"]["chunk_locales"] = dep_job.attributes.get(
            "chunk_locales", ["en-US"]
        )

        yield job


@transforms.add
def filter_out_macos_jobs_but_mac_only_locales(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        build_platform = dep_job.attributes.get("build_platform")

        if build_platform in ("linux64-devedition", "linux64-shippable"):
            yield job
        elif (
            build_platform in ("macosx64-devedition", "macosx64-shippable")
            and "ja-JP-mac" in job["attributes"]["chunk_locales"]
        ):
            # Other locales of the same job shouldn't be processed
            job["attributes"]["chunk_locales"] = ["ja-JP-mac"]
            job["label"] = job["label"].replace(
                # Guard against a chunk 10 or chunk 1 (latter on try) weird munging
                "-{}/".format(job["attributes"]["l10n_chunk"]),
                "-ja-JP-mac/",
            )
            yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        treeherder = inherit_treeherder_from_dep(job, dep_job)
        treeherder.setdefault(
            "symbol", "langpack(SnP{})".format(job["attributes"].get("l10n_chunk", ""))
        )

        job["description"] = job["description"].format(
            locales="/".join(job["attributes"]["chunk_locales"]),
        )

        job["dependencies"] = {dep_job.kind: dep_job.label}
        job["treeherder"] = treeherder

        yield job


def generate_upstream_artifacts(upstream_task_ref, locales):
    return [
        {
            "taskId": {"task-reference": upstream_task_ref},
            "taskType": "build",
            "paths": [
                "public/build{locale}/target.langpack.xpi".format(
                    locale="" if locale == "en-US" else "/" + locale
                )
                for locale in locales
            ],
        }
    ]


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        upstream_task_ref = get_upstream_task_ref(
            job, expected_kinds=("build", "shippable-l10n")
        )

        job["worker"]["implementation"] = "push-addons"
        job["worker"]["upstream-artifacts"] = generate_upstream_artifacts(
            upstream_task_ref, job["attributes"]["chunk_locales"]
        )

        yield job


def get_upstream_task_ref(job, expected_kinds):
    upstream_tasks = [
        job_kind
        for job_kind in job["dependencies"].keys()
        if job_kind in expected_kinds
    ]

    if len(upstream_tasks) > 1:
        raise Exception("Only one dependency expected")

    return f"<{upstream_tasks[0]}>"
