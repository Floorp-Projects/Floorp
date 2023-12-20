# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
from io import BytesIO

from taskgraph.docker import load_image_by_task_id
from taskgraph.generator import load_tasks_for_kind
from taskgraph.optimize.strategies import IndexSearch
from taskgraph.parameters import Parameters

from gecko_taskgraph.util import docker

from . import GECKO


def get_image_digest(image_name):
    params = Parameters(
        level=os.environ.get("MOZ_SCM_LEVEL", "3"),
        strict=False,
    )
    tasks = load_tasks_for_kind(params, "docker-image")
    task = tasks[f"docker-image-{image_name}"]
    return task.attributes["cached_task"]["digest"]


def load_image_by_name(image_name, tag=None):
    params = {"level": os.environ.get("MOZ_SCM_LEVEL", "3")}
    tasks = load_tasks_for_kind(params, "docker-image")
    task = tasks[f"docker-image-{image_name}"]
    deadline = None
    task_id = IndexSearch().should_replace_task(
        task, {}, deadline, task.optimization.get("index-search", [])
    )

    if task_id in (True, False):
        print(
            "Could not find artifacts for a docker image "
            "named `{image_name}`. Local commits and other changes "
            "in your checkout may cause this error. Try "
            "updating to a fresh checkout of mozilla-central "
            "to download image.".format(image_name=image_name)
        )
        return False

    return load_image_by_task_id(task_id, tag)


def build_context(name, outputFile, args=None):
    """Build a context.tar for image with specified name."""
    if not name:
        raise ValueError("must provide a Docker image name")
    if not outputFile:
        raise ValueError("must provide a outputFile")

    image_dir = docker.image_path(name)
    if not os.path.isdir(image_dir):
        raise Exception("image directory does not exist: %s" % image_dir)

    docker.create_context_tar(GECKO, image_dir, outputFile, image_name=name, args=args)


def build_image(name, tag, args=None):
    """Build a Docker image of specified name.

    Output from image building process will be printed to stdout.
    """
    if not name:
        raise ValueError("must provide a Docker image name")

    image_dir = docker.image_path(name)
    if not os.path.isdir(image_dir):
        raise Exception("image directory does not exist: %s" % image_dir)

    tag = tag or docker.docker_image(name, by_tag=True)

    buf = BytesIO()
    docker.stream_context_tar(GECKO, image_dir, buf, name, args)
    docker.post_to_docker(buf.getvalue(), "/build", nocache=1, t=tag)

    print(f"Successfully built {name} and tagged with {tag}")

    if tag.endswith(":latest"):
        print("*" * 50)
        print("WARNING: no VERSION file found in image directory.")
        print("Image is not suitable for deploying/pushing.")
        print("Create an image suitable for deploying/pushing by creating")
        print("a VERSION file in the image directory.")
        print("*" * 50)
