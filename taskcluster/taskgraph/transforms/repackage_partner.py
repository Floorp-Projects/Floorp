# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy

from six import text_type
from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import (
    optionally_keyed_by,
    resolve_keyed_by,
)
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.partners import get_partner_config_by_kind
from taskgraph.util.platforms import archive_format, executable_extension
from taskgraph.util.workertypes import worker_type_implementation
from taskgraph.transforms.task import task_description_schema
from taskgraph.transforms.repackage import PACKAGE_FORMATS as PACKAGE_FORMATS_VANILLA
from voluptuous import Required, Optional


def _by_platform(arg):
    return optionally_keyed_by("build-platform", arg)


# When repacking the stub installer we need to pass a zip file and package name to the
# repackage task. This is not needed for vanilla stub but analogous to the full installer.
PACKAGE_FORMATS = copy.deepcopy(PACKAGE_FORMATS_VANILLA)
PACKAGE_FORMATS["installer-stub"]["inputs"]["package"] = "target-stub{archive_format}"
PACKAGE_FORMATS["installer-stub"]["args"].extend(["--package-name", "{package-name}"])

packaging_description_schema = schema.extend(
    {
        # unique label to describe this repackaging task
        Optional("label"): text_type,
        # Routes specific to this task, if defined
        Optional("routes"): [text_type],
        # passed through directly to the job description
        Optional("extra"): task_description_schema["extra"],
        # Shipping product and phase
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Required("package-formats"): _by_platform([text_type]),
        # All l10n jobs use mozharness
        Required("mozharness"): {
            # Config files passed to the mozharness script
            Required("config"): _by_platform([text_type]),
            # Additional paths to look for mozharness configs in. These should be
            # relative to the base of the source checkout
            Optional("config-paths"): [text_type],
            # if true, perform a checkout of a comm-central based branch inside the
            # gecko checkout
            Optional("comm-checkout"): bool,
        },
        # Override the default priority for the project
        Optional("priority"): task_description_schema["priority"],
    }
)

transforms = TransformSequence()
transforms.add_validate(packaging_description_schema)


@transforms.add
def copy_in_useful_magic(config, jobs):
    """Copy attributes from upstream task to be used for keyed configuration."""
    for job in jobs:
        dep = job["primary-dependency"]
        job["build-platform"] = dep.attributes.get("build_platform")
        yield job


@transforms.add
def handle_keyed_by(config, jobs):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        "mozharness.config",
        "package-formats",
    ]
    for job in jobs:
        job = copy.deepcopy(job)  # don't overwrite dict values here
        for field in fields:
            resolve_keyed_by(item=job, field=field, item_name="?")
        yield job


@transforms.add
def make_repackage_description(config, jobs):
    for job in jobs:
        dep_job = job["primary-dependency"]

        label = job.get("label", dep_job.label.replace("signing-", "repackage-"))
        job["label"] = label

        yield job


@transforms.add
def make_job_description(config, jobs):
    for job in jobs:
        dep_job = job["primary-dependency"]
        attributes = copy_attributes_from_dependent_job(dep_job)
        build_platform = attributes["build_platform"]

        if job["build-platform"].startswith("win"):
            if dep_job.kind.endswith("signing"):
                continue
        if job["build-platform"].startswith("macosx"):
            if dep_job.kind.endswith("repack"):
                continue
        dependencies = {dep_job.attributes.get("kind"): dep_job.label}
        dependencies.update(dep_job.dependencies)

        signing_task = None
        for dependency in dependencies.keys():
            if build_platform.startswith("macosx") and dependency.endswith("signing"):
                signing_task = dependency
            elif build_platform.startswith("win") and dependency.endswith("repack"):
                signing_task = dependency

        attributes["repackage_type"] = "repackage"

        repack_id = job["extra"]["repack_id"]

        partner_config = get_partner_config_by_kind(config, config.kind)
        partner, subpartner, _ = repack_id.split("/")
        repack_stub_installer = partner_config[partner][subpartner].get(
            "repack_stub_installer"
        )
        if build_platform.startswith("win32") and repack_stub_installer:
            job["package-formats"].append("installer-stub")

        repackage_config = []
        for format in job.get("package-formats"):
            command = copy.deepcopy(PACKAGE_FORMATS[format])
            substs = {
                "archive_format": archive_format(build_platform),
                "executable_extension": executable_extension(build_platform),
            }
            command["inputs"] = {
                name: filename.format(**substs)
                for name, filename in command["inputs"].items()
            }
            repackage_config.append(command)

        run = job.get("mozharness", {})
        run.update(
            {
                "using": "mozharness",
                "script": "mozharness/scripts/repackage.py",
                "job-script": "taskcluster/scripts/builder/repackage.sh",
                "actions": ["setup", "repackage"],
                "extra-config": {
                    "repackage_config": repackage_config,
                },
            }
        )

        worker = {
            "chain-of-trust": True,
            "max-run-time": 7200 if build_platform.startswith("win") else 3600,
            "taskcluster-proxy": True if get_artifact_prefix(dep_job) else False,
            "env": {
                "REPACK_ID": repack_id,
            },
            # Don't add generic artifact directory.
            "skip-artifacts": True,
        }

        worker_type = "b-linux"
        worker["docker-image"] = {"in-tree": "debian11-amd64-build"}

        worker["artifacts"] = _generate_task_output_files(
            dep_job,
            worker_type_implementation(config.graph_config, worker_type),
            repackage_config,
            partner=repack_id,
        )

        description = (
            "Repackaging for repack_id '{repack_id}' for build '"
            "{build_platform}/{build_type}'".format(
                repack_id=job["extra"]["repack_id"],
                build_platform=attributes.get("build_platform"),
                build_type=attributes.get("build_type"),
            )
        )

        task = {
            "label": job["label"],
            "description": description,
            "worker-type": worker_type,
            "dependencies": dependencies,
            "attributes": attributes,
            "scopes": ["queue:get-artifact:releng/partner/*"],
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "routes": job.get("routes", []),
            "extra": job.get("extra", {}),
            "worker": worker,
            "run": run,
            "fetches": _generate_download_config(
                dep_job,
                build_platform,
                signing_task,
                partner=repack_id,
                project=config.params["project"],
                repack_stub_installer=repack_stub_installer,
            ),
        }

        # we may have reduced the priority for partner jobs, otherwise task.py will set it
        if job.get("priority"):
            task["priority"] = job["priority"]
        if build_platform.startswith("macosx"):
            task.setdefault("fetches", {}).setdefault("toolchain", []).extend(
                [
                    "linux64-libdmg",
                    "linux64-hfsplus",
                    "linux64-node",
                ]
            )
        yield task


def _generate_download_config(
    task,
    build_platform,
    signing_task,
    partner=None,
    project=None,
    repack_stub_installer=False,
):
    locale_path = "{}/".format(partner) if partner else ""

    if build_platform.startswith("macosx"):
        return {
            signing_task: [
                {
                    "artifact": "{}target.tar.gz".format(locale_path),
                    "extract": False,
                },
            ],
        }
    elif build_platform.startswith("win"):
        download_config = [
            {
                "artifact": "{}target.zip".format(locale_path),
                "extract": False,
            },
            "{}setup.exe".format(locale_path),
        ]
        if build_platform.startswith("win32") and repack_stub_installer:
            download_config.extend(
                [
                    {
                        "artifact": "{}target-stub.zip".format(locale_path),
                        "extract": False,
                    },
                    "{}setup-stub.exe".format(locale_path),
                ]
            )
        return {signing_task: download_config}

    raise NotImplementedError('Unsupported build_platform: "{}"'.format(build_platform))


def _generate_task_output_files(task, worker_implementation, repackage_config, partner):
    """We carefully generate an explicit list here, but there's an artifacts directory
    too, courtesy of generic_worker_add_artifacts() (windows) or docker_worker_add_artifacts().
    Any errors here are likely masked by that.
    """
    partner_output_path = "{}/".format(partner)
    artifact_prefix = get_artifact_prefix(task)

    if worker_implementation == ("docker-worker", "linux"):
        local_prefix = "/builds/worker/workspace/"
    elif worker_implementation == ("generic-worker", "windows"):
        local_prefix = "workspace/"
    else:
        raise NotImplementedError(
            'Unsupported worker implementation: "{}"'.format(worker_implementation)
        )

    output_files = []
    for config in repackage_config:
        output_files.append(
            {
                "type": "file",
                "path": "{}outputs/{}{}".format(
                    local_prefix, partner_output_path, config["output"]
                ),
                "name": "{}/{}{}".format(
                    artifact_prefix, partner_output_path, config["output"]
                ),
            }
        )
    return output_files
