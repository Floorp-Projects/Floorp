# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Do transforms specific to l10n kind
"""


import json

from mozbuild.chunkify import chunkify
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.copy import deepcopy
from taskgraph.util.dependencies import get_dependencies, get_primary_dependency
from taskgraph.util.schema import (
    Schema,
    optionally_keyed_by,
    resolve_keyed_by,
    taskref_or_string,
)
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.treeherder import add_suffix
from voluptuous import Any, Optional, Required

from gecko_taskgraph.transforms.job import job_description_schema
from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import (
    copy_attributes_from_dependent_job,
    sorted_unique_list,
    task_name,
)


def _by_platform(arg):
    return optionally_keyed_by("build-platform", arg)


l10n_description_schema = Schema(
    {
        # Name for this job, inferred from the dependent job before validation
        Required("name"): str,
        # build-platform, inferred from dependent job before validation
        Required("build-platform"): str,
        # max run time of the task
        Required("run-time"): _by_platform(int),
        # Locales not to repack for
        Required("ignore-locales"): _by_platform([str]),
        # All l10n jobs use mozharness
        Required("mozharness"): {
            # Script to invoke for mozharness
            Required("script"): _by_platform(str),
            # Config files passed to the mozharness script
            Required("config"): _by_platform([str]),
            # Additional paths to look for mozharness configs in. These should be
            # relative to the base of the source checkout
            Optional("config-paths"): [str],
            # Options to pass to the mozharness script
            Optional("options"): _by_platform([str]),
            # Action commands to provide to mozharness script
            Required("actions"): _by_platform([str]),
            # if true, perform a checkout of a comm-central based branch inside the
            # gecko checkout
            Optional("comm-checkout"): bool,
        },
        # Items for the taskcluster index
        Optional("index"): {
            # Product to identify as in the taskcluster index
            Required("product"): _by_platform(str),
            # Job name to identify as in the taskcluster index
            Required("job-name"): _by_platform(str),
            # Type of index
            Optional("type"): _by_platform(str),
        },
        # Description of the localized task
        Required("description"): _by_platform(str),
        Optional("run-on-projects"): job_description_schema["run-on-projects"],
        # worker-type to utilize
        Required("worker-type"): _by_platform(str),
        # File which contains the used locales
        Required("locales-file"): _by_platform(str),
        # Tooltool visibility required for task.
        Required("tooltool"): _by_platform(Any("internal", "public")),
        # Docker image required for task.  We accept only in-tree images
        # -- generally desktop-build or android-build -- for now.
        Optional("docker-image"): _by_platform(
            # an in-tree generated docker image (from `taskcluster/docker/<name>`)
            {"in-tree": str},
        ),
        Optional("fetches"): {
            str: _by_platform([str]),
        },
        # The set of secret names to which the task has access; these are prefixed
        # with `project/releng/gecko/{treeherder.kind}/level-{level}/`.  Setting
        # this will enable any worker features required and set the task's scopes
        # appropriately.  `true` here means ['*'], all secrets.  Not supported on
        # Windows
        Optional("secrets"): _by_platform(Any(bool, [str])),
        # Information for treeherder
        Required("treeherder"): {
            # Platform to display the task on in treeherder
            Required("platform"): _by_platform(str),
            # Symbol to use
            Required("symbol"): str,
            # Tier this task is
            Required("tier"): _by_platform(int),
        },
        # Extra environment values to pass to the worker
        Optional("env"): _by_platform({str: taskref_or_string}),
        # Max number locales per chunk
        Optional("locales-per-chunk"): _by_platform(int),
        # Task deps to chain this task with, added in transforms from primary dependency
        # if this is a shippable-style build
        Optional("dependencies"): {str: str},
        # Run the task when the listed files change (if present).
        Optional("when"): {"files-changed": [str]},
        # passed through directly to the job description
        Optional("attributes"): job_description_schema["attributes"],
        Optional("extra"): job_description_schema["extra"],
        # Shipping product and phase
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("job-from"): task_description_schema["job-from"],
    }
)

transforms = TransformSequence()


def parse_locales_file(locales_file, platform=None):
    """Parse the passed locales file for a list of locales."""
    locales = []

    with open(locales_file, mode="r") as f:
        if locales_file.endswith("json"):
            all_locales = json.load(f)
            # XXX Only single locales are fetched
            locales = {
                locale: data["revision"]
                for locale, data in all_locales.items()
                if platform is None or platform in data["platforms"]
            }
        else:
            all_locales = f.read().split()
            # 'default' is the hg revision at the top of hg repo, in this context
            locales = {locale: "default" for locale in all_locales}
    return locales


def _remove_locales(locales, to_remove=None):
    # ja-JP-mac is a mac-only locale, but there are no mac builds being repacked,
    # so just omit it unconditionally
    return {
        locale: revision
        for locale, revision in locales.items()
        if locale not in to_remove
    }


@transforms.add
def setup_name(config, jobs):
    for job in jobs:
        dep = get_primary_dependency(config, job)
        assert dep
        # Set the name to the same as the dep task, without kind name.
        # Label will get set automatically with this kinds name.
        job["name"] = job.get("name", task_name(dep))
        yield job


@transforms.add
def copy_in_useful_magic(config, jobs):
    for job in jobs:
        dep = get_primary_dependency(config, job)
        assert dep
        attributes = copy_attributes_from_dependent_job(dep)
        attributes.update(job.get("attributes", {}))
        # build-platform is needed on `job` for by-build-platform
        job["build-platform"] = attributes.get("build_platform")
        job["attributes"] = attributes
        yield job


transforms.add_validate(l10n_description_schema)


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
def remove_repackage_dependency(config, jobs):
    for job in jobs:
        build_platform = job["attributes"]["build_platform"]
        if not build_platform.startswith("macosx"):
            del job["dependencies"]["repackage"]

        yield job


@transforms.add
def handle_keyed_by(config, jobs):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        "locales-file",
        "locales-per-chunk",
        "worker-type",
        "description",
        "run-time",
        "docker-image",
        "secrets",
        "fetches.toolchain",
        "fetches.fetch",
        "tooltool",
        "env",
        "ignore-locales",
        "mozharness.config",
        "mozharness.options",
        "mozharness.actions",
        "mozharness.script",
        "treeherder.tier",
        "treeherder.platform",
        "index.type",
        "index.product",
        "index.job-name",
        "when.files-changed",
    ]
    for job in jobs:
        job = deepcopy(job)  # don't overwrite dict values here
        for field in fields:
            resolve_keyed_by(item=job, field=field, item_name=job["name"])
        yield job


@transforms.add
def handle_artifact_prefix(config, jobs):
    """Resolve ``artifact_prefix`` in env vars"""
    for job in jobs:
        artifact_prefix = get_artifact_prefix(job)
        for k1, v1 in job.get("env", {}).items():
            if isinstance(v1, str):
                job["env"][k1] = v1.format(artifact_prefix=artifact_prefix)
            elif isinstance(v1, dict):
                for k2, v2 in v1.items():
                    job["env"][k1][k2] = v2.format(artifact_prefix=artifact_prefix)
        yield job


@transforms.add
def all_locales_attribute(config, jobs):
    for job in jobs:
        locales_platform = job["attributes"]["build_platform"].replace("-shippable", "")
        locales_platform = locales_platform.replace("-pgo", "")
        locales_with_changesets = parse_locales_file(
            job["locales-file"], platform=locales_platform
        )
        locales_with_changesets = _remove_locales(
            locales_with_changesets, to_remove=job["ignore-locales"]
        )

        locales = sorted(locales_with_changesets.keys())
        attributes = job.setdefault("attributes", {})
        attributes["all_locales"] = locales
        attributes["all_locales_with_changesets"] = locales_with_changesets
        if job.get("shipping-product"):
            attributes["shipping_product"] = job["shipping-product"]
        yield job


@transforms.add
def chunk_locales(config, jobs):
    """Utilizes chunking for l10n stuff"""
    for job in jobs:
        locales_per_chunk = job.get("locales-per-chunk")
        locales_with_changesets = job["attributes"]["all_locales_with_changesets"]
        if locales_per_chunk:
            chunks, remainder = divmod(len(locales_with_changesets), locales_per_chunk)
            if remainder:
                chunks = int(chunks + 1)
            for this_chunk in range(1, chunks + 1):
                chunked = deepcopy(job)
                chunked["name"] = chunked["name"].replace("/", f"-{this_chunk}/", 1)
                chunked["mozharness"]["options"] = chunked["mozharness"].get(
                    "options", []
                )
                # chunkify doesn't work with dicts
                locales_with_changesets_as_list = sorted(
                    locales_with_changesets.items()
                )
                chunked_locales = chunkify(
                    locales_with_changesets_as_list, this_chunk, chunks
                )
                chunked["mozharness"]["options"].extend(
                    [
                        f"locale={locale}:{changeset}"
                        for locale, changeset in chunked_locales
                    ]
                )
                chunked["attributes"]["l10n_chunk"] = str(this_chunk)
                # strip revision
                chunked["attributes"]["chunk_locales"] = [
                    locale for locale, _ in chunked_locales
                ]

                # add the chunk number to the TH symbol
                chunked["treeherder"]["symbol"] = add_suffix(
                    chunked["treeherder"]["symbol"], this_chunk
                )
                yield chunked
        else:
            job["mozharness"]["options"] = job["mozharness"].get("options", [])
            job["mozharness"]["options"].extend(
                [
                    f"locale={locale}:{changeset}"
                    for locale, changeset in sorted(locales_with_changesets.items())
                ]
            )
            yield job


transforms.add_validate(l10n_description_schema)


@transforms.add
def stub_installer(config, jobs):
    for job in jobs:
        job.setdefault("attributes", {})
        job.setdefault("env", {})
        if job["attributes"].get("stub-installer"):
            job["env"].update({"USE_STUB_INSTALLER": "1"})
        yield job


@transforms.add
def set_extra_config(config, jobs):
    for job in jobs:
        job["mozharness"].setdefault("extra-config", {})["branch"] = config.params[
            "project"
        ]
        if "update-channel" in job["attributes"]:
            job["mozharness"]["extra-config"]["update_channel"] = job["attributes"][
                "update-channel"
            ]
        yield job


@transforms.add
def make_job_description(config, jobs):
    for job in jobs:
        job["mozharness"].update(
            {
                "using": "mozharness",
                "job-script": "taskcluster/scripts/builder/build-l10n.sh",
                "secrets": job.get("secrets", False),
            }
        )
        job_description = {
            "name": job["name"],
            "worker-type": job["worker-type"],
            "description": job["description"],
            "run": job["mozharness"],
            "attributes": job["attributes"],
            "treeherder": {
                "kind": "build",
                "tier": job["treeherder"]["tier"],
                "symbol": job["treeherder"]["symbol"],
                "platform": job["treeherder"]["platform"],
            },
            "run-on-projects": job.get("run-on-projects")
            if job.get("run-on-projects")
            else [],
        }
        if job.get("extra"):
            job_description["extra"] = job["extra"]

        job_description["run"]["tooltool-downloads"] = job["tooltool"]

        job_description["worker"] = {
            "max-run-time": job["run-time"],
            "chain-of-trust": True,
        }
        if job["worker-type"] in ["b-win2012", "b-win2022"]:
            job_description["worker"]["os"] = "windows"
            job_description["run"]["use-simple-package"] = False
            job_description["run"]["use-magic-mh-args"] = False

        if job.get("docker-image"):
            job_description["worker"]["docker-image"] = job["docker-image"]

        if job.get("fetches"):
            job_description["fetches"] = job["fetches"]

        if job.get("index"):
            job_description["index"] = {
                "product": job["index"]["product"],
                "job-name": job["index"]["job-name"],
                "type": job["index"].get("type", "generic"),
            }

        if job.get("dependencies"):
            job_description["dependencies"] = job["dependencies"]
        if job.get("env"):
            job_description["worker"]["env"] = job["env"]
        if job.get("when", {}).get("files-changed"):
            job_description.setdefault("when", {})
            job_description["when"]["files-changed"] = [job["locales-file"]] + job[
                "when"
            ]["files-changed"]

        if "shipping-phase" in job:
            job_description["shipping-phase"] = job["shipping-phase"]

        if "shipping-product" in job:
            job_description["shipping-product"] = job["shipping-product"]

        yield job_description
