# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

from ..build_config import CHECKSUMS_EXTENSIONS


transforms = TransformSequence()


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        for key in ("index", "worker-type", "worker.signing-type", "treeherder.job-symbol"):
            resolve_keyed_by(
                task, key,
                item_name=task["name"],
                **{
                    "build-type": task["attributes"]["build-type"],
                    "level": config.params["level"],
                }
            )
        yield task


@transforms.add
def set_signing_attributes(config, tasks):
    for task in tasks:
        task["attributes"]["signed"] = True
        yield task


@transforms.add
def filter_out_checksums(config, tasks):
    for task in tasks:
        task["attributes"]["artifacts"] = {
            extension: path
            for extension, path in task["attributes"]["artifacts"].items()
            if not any(map(path.endswith, CHECKSUMS_EXTENSIONS))
        }

        for upstream_artifact in task["worker"]["upstream-artifacts"]:
            upstream_artifact["paths"] = [
                path
                for path in upstream_artifact["paths"]
                if not any(map(path.endswith, CHECKSUMS_EXTENSIONS))
            ]

        yield task


_DETACHED_SIGNATURE_EXTENSION = '.asc'


@transforms.add
def set_detached_signature_artifacts(config, tasks):
    for task in tasks:
        task["attributes"]["artifacts"] = {
            extension + _DETACHED_SIGNATURE_EXTENSION: path + _DETACHED_SIGNATURE_EXTENSION
            for extension, path in task["attributes"]["artifacts"].items()
        }

        yield task


@transforms.add
def set_signing_format(config, tasks):
    for task in tasks:
        for upstream_artifact in task["worker"]["upstream-artifacts"]:
            upstream_artifact["formats"] = ["autograph_gpg"]

        yield task


@transforms.add
def remove_dependent_tasks(config, tasks):
    for task in tasks:
        del task["dependent-tasks"]
        yield task
