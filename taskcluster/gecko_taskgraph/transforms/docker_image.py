# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import os
import re
import json

import mozpack.path as mozpath
import taskgraph
from taskgraph.util.schema import Schema

from gecko_taskgraph.transforms.base import TransformSequence
from .. import GECKO
from gecko_taskgraph.util.docker import (
    create_context_tar,
    generate_context_hash,
    image_path,
)
from voluptuous import (
    Optional,
    Required,
)
from .task import task_description_schema

logger = logging.getLogger(__name__)

CONTEXTS_DIR = "docker-contexts"

DIGEST_RE = re.compile("^[0-9a-f]{64}$")

IMAGE_BUILDER_IMAGE = (
    "mozillareleases/image_builder:5.0.0"
    "@sha256:"
    "e510a9a9b80385f71c112d61b2f2053da625aff2b6d430411ac42e424c58953f"
)

transforms = TransformSequence()

docker_image_schema = Schema(
    {
        # Name of the docker image.
        Required("name"): str,
        # Name of the parent docker image.
        Optional("parent"): str,
        # Treeherder symbol.
        Required("symbol"): str,
        # relative path (from config.path) to the file the docker image was defined
        # in.
        Optional("job-from"): str,
        # Arguments to use for the Dockerfile.
        Optional("args"): {str: str},
        # Name of the docker image definition under taskcluster/docker, when
        # different from the docker image name.
        Optional("definition"): str,
        # List of package tasks this docker image depends on.
        Optional("packages"): [str],
        Optional(
            "index",
            description="information for indexing this build so its artifacts can be discovered",
        ): task_description_schema["index"],
        Optional(
            "cache",
            description="Whether this image should be cached based on inputs.",
        ): bool,
    }
)


transforms.add_validate(docker_image_schema)


@transforms.add
def fill_template(config, tasks):
    if not taskgraph.fast and config.write_artifacts:
        if not os.path.isdir(CONTEXTS_DIR):
            os.makedirs(CONTEXTS_DIR)

    for task in tasks:
        image_name = task.pop("name")
        job_symbol = task.pop("symbol")
        args = task.pop("args", {})
        packages = task.pop("packages", [])
        parent = task.pop("parent", None)

        for p in packages:
            if f"packages-{p}" not in config.kind_dependencies_tasks:
                raise Exception(
                    "Missing package job for {}-{}: {}".format(
                        config.kind, image_name, p
                    )
                )

        if not taskgraph.fast:
            context_path = mozpath.relpath(image_path(image_name), GECKO)
            if config.write_artifacts:
                context_file = os.path.join(CONTEXTS_DIR, f"{image_name}.tar.gz")
                logger.info(f"Writing {context_file} for docker image {image_name}")
                context_hash = create_context_tar(
                    GECKO, context_path, context_file, image_name, args
                )
            else:
                context_hash = generate_context_hash(
                    GECKO, context_path, image_name, args
                )
        else:
            if config.write_artifacts:
                raise Exception("Can't write artifacts if `taskgraph.fast` is set.")
            context_hash = "0" * 40
        digest_data = [context_hash]
        digest_data += [json.dumps(args, sort_keys=True)]

        description = "Build the docker image {} for use by dependent tasks".format(
            image_name
        )

        args["DOCKER_IMAGE_PACKAGES"] = " ".join(f"<{p}>" for p in packages)

        # Adjust the zstandard compression level based on the execution level.
        # We use faster compression for level 1 because we care more about
        # end-to-end times. We use slower/better compression for other levels
        # because images are read more often and it is worth the trade-off to
        # burn more CPU once to reduce image size.
        zstd_level = "3" if int(config.params["level"]) == 1 else "10"

        # include some information that is useful in reconstructing this task
        # from JSON
        taskdesc = {
            "label": f"{config.kind}-{image_name}",
            "description": description,
            "attributes": {
                "image_name": image_name,
                "artifact_prefix": "public",
            },
            "expires-after": "1 year",
            "scopes": [],
            "treeherder": {
                "symbol": job_symbol,
                "platform": "taskcluster-images/opt",
                "kind": "other",
                "tier": 1,
            },
            "run-on-projects": [],
            "worker-type": "images",
            "worker": {
                "implementation": "docker-worker",
                "os": "linux",
                "artifacts": [
                    {
                        "type": "file",
                        "path": "/workspace/image.tar.zst",
                        "name": "public/image.tar.zst",
                    }
                ],
                "env": {
                    "CONTEXT_TASK_ID": {"task-reference": "<decision>"},
                    "CONTEXT_PATH": "public/docker-contexts/{}.tar.gz".format(
                        image_name
                    ),
                    "HASH": context_hash,
                    "PROJECT": config.params["project"],
                    "IMAGE_NAME": image_name,
                    "DOCKER_IMAGE_ZSTD_LEVEL": zstd_level,
                    "DOCKER_BUILD_ARGS": {"task-reference": json.dumps(args)},
                    "GECKO_BASE_REPOSITORY": config.params["base_repository"],
                    "GECKO_HEAD_REPOSITORY": config.params["head_repository"],
                    "GECKO_HEAD_REV": config.params["head_rev"],
                },
                "chain-of-trust": True,
                "max-run-time": 7200,
                # FIXME: We aren't currently propagating the exit code
            },
        }
        # Retry for 'funsize-update-generator' if exit status code is -1
        if image_name in ["funsize-update-generator"]:
            taskdesc["worker"]["retry-exit-status"] = [-1]

        worker = taskdesc["worker"]

        if image_name == "image_builder":
            worker["docker-image"] = IMAGE_BUILDER_IMAGE
            digest_data.append(f"image-builder-image:{IMAGE_BUILDER_IMAGE}")
        else:
            worker["docker-image"] = {"in-tree": "image_builder"}
            deps = taskdesc.setdefault("dependencies", {})
            deps["docker-image"] = f"{config.kind}-image_builder"

        if packages:
            deps = taskdesc.setdefault("dependencies", {})
            for p in sorted(packages):
                deps[p] = f"packages-{p}"

        if parent:
            deps = taskdesc.setdefault("dependencies", {})
            deps["parent"] = f"{config.kind}-{parent}"
            worker["env"]["PARENT_TASK_ID"] = {
                "task-reference": "<parent>",
            }
        if "index" in task:
            taskdesc["index"] = task["index"]

        if task.get("cache", True) and not taskgraph.fast:
            taskdesc["cache"] = {
                "type": "docker-images.v2",
                "name": image_name,
                "digest-data": digest_data,
            }

        yield taskdesc
