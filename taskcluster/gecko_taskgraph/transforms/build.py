# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""
import logging

from mozbuild.artifact_builds import JOB_CHOICES as ARTIFACT_JOBS
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.treeherder import add_suffix

from gecko_taskgraph.util.attributes import RELEASE_PROJECTS, is_try, release_level
from gecko_taskgraph.util.workertypes import worker_type_implementation

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def set_defaults(config, jobs):
    """Set defaults, including those that differ per worker implementation"""
    for job in jobs:
        job["treeherder"].setdefault("kind", "build")
        job["treeherder"].setdefault("tier", 1)
        _, worker_os = worker_type_implementation(
            config.graph_config, config.params, job["worker-type"]
        )
        worker = job.setdefault("worker", {})
        worker.setdefault("env", {})
        worker["chain-of-trust"] = True
        yield job


@transforms.add
def stub_installer(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job,
            "stub-installer",
            item_name=job["name"],
            project=config.params["project"],
            **{
                "release-type": config.params["release_type"],
            },
        )
        job.setdefault("attributes", {})
        if job.get("stub-installer"):
            job["attributes"]["stub-installer"] = job["stub-installer"]
            job["worker"]["env"].update({"USE_STUB_INSTALLER": "1"})
        if "stub-installer" in job:
            del job["stub-installer"]
        yield job


@transforms.add
def resolve_shipping_product(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job,
            "shipping-product",
            item_name=job["name"],
            **{
                "release-type": config.params["release_type"],
            },
        )
        yield job


@transforms.add
def update_channel(config, jobs):
    keys = [
        "run.update-channel",
        "run.mar-channel-id",
        "run.accepted-mar-channel-ids",
    ]
    for job in jobs:
        job["worker"].setdefault("env", {})
        for key in keys:
            resolve_keyed_by(
                job,
                key,
                item_name=job["name"],
                **{
                    "project": config.params["project"],
                    "release-type": config.params["release_type"],
                },
            )
        update_channel = job["run"].pop("update-channel", None)
        if update_channel:
            job["run"].setdefault("extra-config", {})["update_channel"] = update_channel
            job["attributes"]["update-channel"] = update_channel
        mar_channel_id = job["run"].pop("mar-channel-id", None)
        if mar_channel_id:
            job["attributes"]["mar-channel-id"] = mar_channel_id
            job["worker"]["env"]["MAR_CHANNEL_ID"] = mar_channel_id
        accepted_mar_channel_ids = job["run"].pop("accepted-mar-channel-ids", None)
        if accepted_mar_channel_ids:
            job["attributes"]["accepted-mar-channel-ids"] = accepted_mar_channel_ids
            job["worker"]["env"]["ACCEPTED_MAR_CHANNEL_IDS"] = accepted_mar_channel_ids

        yield job


@transforms.add
def mozconfig(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job,
            "run.mozconfig-variant",
            item_name=job["name"],
            **{
                "release-type": config.params["release_type"],
            },
        )
        mozconfig_variant = job["run"].pop("mozconfig-variant", None)
        if mozconfig_variant:
            job["run"].setdefault("extra-config", {})[
                "mozconfig_variant"
            ] = mozconfig_variant
        yield job


@transforms.add
def use_artifact(config, jobs):
    if is_try(config.params):
        use_artifact = config.params["try_task_config"].get(
            "use-artifact-builds", False
        )
    else:
        use_artifact = False
    for job in jobs:
        if (
            config.kind == "build"
            and use_artifact
            and job.get("index", {}).get("job-name") in ARTIFACT_JOBS
            # If tests aren't packaged, then we are not able to rebuild all the packages
            and job["worker"]["env"].get("MOZ_AUTOMATION_PACKAGE_TESTS") == "1"
        ):
            job["treeherder"]["symbol"] = add_suffix(job["treeherder"]["symbol"], "a")
            job["worker"]["env"]["USE_ARTIFACT"] = "1"
            job["attributes"]["artifact-build"] = True
        yield job


@transforms.add
def use_profile_data(config, jobs):
    for job in jobs:
        use_pgo = job.pop("use-pgo", False)
        disable_pgo = config.params["try_task_config"].get("disable-pgo", False)
        artifact_build = job["attributes"].get("artifact-build")
        if not use_pgo or disable_pgo or artifact_build:
            yield job
            continue

        # If use_pgo is True, the task uses the generate-profile task of the
        # same name. Otherwise a task can specify a specific generate-profile
        # task to use in the use_pgo field.
        if use_pgo is True:
            name = job["name"]
        else:
            name = use_pgo
        dependencies = f"generate-profile-{name}"
        job.setdefault("dependencies", {})["generate-profile"] = dependencies
        job.setdefault("fetches", {})["generate-profile"] = ["profdata.tar.xz"]
        job["worker"]["env"].update({"TASKCLUSTER_PGO_PROFILE_USE": "1"})

        _, worker_os = worker_type_implementation(
            config.graph_config, config.params, job["worker-type"]
        )
        if worker_os == "linux":
            # LTO linkage needs more open files than the default from run-task.
            job["worker"]["env"].update({"MOZ_LIMIT_NOFILE": "8192"})

        if job.get("use-sccache"):
            raise Exception(
                "use-sccache is incompatible with use-pgo in {}".format(job["name"])
            )

        yield job


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job,
            "use-sccache",
            item_name=job["name"],
            **{"release-level": release_level(config.params["project"])},
        )
        yield job


@transforms.add
def enable_full_crashsymbols(config, jobs):
    """Enable full crashsymbols on jobs with
    'enable-full-crashsymbols' set to True and on release branches, or
    on try"""
    branches = RELEASE_PROJECTS | {
        "toolchains",
        "try",
    }
    for job in jobs:
        enable_full_crashsymbols = job["attributes"].get("enable-full-crashsymbols")
        if enable_full_crashsymbols and config.params["project"] in branches:
            logger.debug("Enabling full symbol generation for %s", job["name"])
            job["worker"]["env"]["MOZ_ENABLE_FULL_SYMBOLS"] = "1"
        else:
            logger.debug("Disabling full symbol generation for %s", job["name"])
            job["attributes"].pop("enable-full-crashsymbols", None)
        yield job


@transforms.add
def set_expiry(config, jobs):
    for job in jobs:
        attributes = job["attributes"]
        if (
            "shippable" in attributes
            and attributes["shippable"]
            and config.kind
            in {
                "build",
            }
        ):
            expiration_policy = "long"
        else:
            expiration_policy = "medium"

        job["expiration-policy"] = expiration_policy
        yield job
