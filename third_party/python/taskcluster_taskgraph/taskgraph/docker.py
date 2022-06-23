# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import os
import tarfile
from io import BytesIO

from taskgraph.util import docker
from taskgraph.util.taskcluster import get_artifact_url, get_session


def get_image_digest(image_name):
    from taskgraph.generator import load_tasks_for_kind
    from taskgraph.parameters import Parameters

    params = Parameters(
        level=os.environ.get("MOZ_SCM_LEVEL", "3"),
        strict=False,
    )
    tasks = load_tasks_for_kind(params, "docker-image")
    task = tasks[f"build-docker-image-{image_name}"]
    return task.attributes["cached_task"]["digest"]


def load_image_by_name(image_name, tag=None):
    from taskgraph.generator import load_tasks_for_kind
    from taskgraph.optimize import IndexSearch
    from taskgraph.parameters import Parameters

    params = Parameters(
        level=os.environ.get("MOZ_SCM_LEVEL", "3"),
        strict=False,
    )
    tasks = load_tasks_for_kind(params, "docker-image")
    task = tasks[f"build-docker-image-{image_name}"]
    task_id = IndexSearch().should_replace_task(
        task, {}, task.optimization.get("index-search", [])
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


def load_image_by_task_id(task_id, tag=None):
    artifact_url = get_artifact_url(task_id, "public/image.tar.zst")
    result = load_image(artifact_url, tag)
    print("Found docker image: {}:{}".format(result["image"], result["tag"]))
    if tag:
        print(f"Re-tagged as: {tag}")
    else:
        tag = "{}:{}".format(result["image"], result["tag"])
    print(f"Try: docker run -ti --rm {tag} bash")
    return True


def build_context(name, outputFile, args=None):
    """Build a context.tar for image with specified name."""
    if not name:
        raise ValueError("must provide a Docker image name")
    if not outputFile:
        raise ValueError("must provide a outputFile")

    image_dir = docker.image_path(name)
    if not os.path.isdir(image_dir):
        raise Exception("image directory does not exist: %s" % image_dir)

    docker.create_context_tar(".", image_dir, outputFile, args)


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
    docker.stream_context_tar(".", image_dir, buf, "", args)
    docker.post_to_docker(buf.getvalue(), "/build", nocache=1, t=tag)

    print(f"Successfully built {name} and tagged with {tag}")

    if tag.endswith(":latest"):
        print("*" * 50)
        print("WARNING: no VERSION file found in image directory.")
        print("Image is not suitable for deploying/pushing.")
        print("Create an image suitable for deploying/pushing by creating")
        print("a VERSION file in the image directory.")
        print("*" * 50)


def load_image(url, imageName=None, imageTag=None):
    """
    Load docker image from URL as imageName:tag, if no imageName or tag is given
    it will use whatever is inside the zstd compressed tarball.

    Returns an object with properties 'image', 'tag' and 'layer'.
    """
    import zstandard as zstd

    # If imageName is given and we don't have an imageTag
    # we parse out the imageTag from imageName, or default it to 'latest'
    # if no imageName and no imageTag is given, 'repositories' won't be rewritten
    if imageName and not imageTag:
        if ":" in imageName:
            imageName, imageTag = imageName.split(":", 1)
        else:
            imageTag = "latest"

    info = {}

    def download_and_modify_image():
        # This function downloads and edits the downloaded tar file on the fly.
        # It emits chunked buffers of the edited tar file, as a generator.
        print(f"Downloading from {url}")
        # get_session() gets us a requests.Session set to retry several times.
        req = get_session().get(url, stream=True)
        req.raise_for_status()

        with zstd.ZstdDecompressor().stream_reader(req.raw) as ifh:

            tarin = tarfile.open(
                mode="r|",
                fileobj=ifh,
                bufsize=zstd.DECOMPRESSION_RECOMMENDED_OUTPUT_SIZE,
            )

            # Stream through each member of the downloaded tar file individually.
            for member in tarin:
                # Non-file members only need a tar header. Emit one.
                if not member.isfile():
                    yield member.tobuf(tarfile.GNU_FORMAT)
                    continue

                # Open stream reader for the member
                reader = tarin.extractfile(member)

                # If member is `repositories`, we parse and possibly rewrite the
                # image tags.
                if member.name == "repositories":
                    # Read and parse repositories
                    repos = json.loads(reader.read())
                    reader.close()

                    # If there is more than one image or tag, we can't handle it
                    # here.
                    if len(repos.keys()) > 1:
                        raise Exception("file contains more than one image")
                    info["image"] = image = list(repos.keys())[0]
                    if len(repos[image].keys()) > 1:
                        raise Exception("file contains more than one tag")
                    info["tag"] = tag = list(repos[image].keys())[0]
                    info["layer"] = layer = repos[image][tag]

                    # Rewrite the repositories file
                    data = json.dumps({imageName or image: {imageTag or tag: layer}})
                    reader = BytesIO(data.encode("utf-8"))
                    member.size = len(data)

                # Emit the tar header for this member.
                yield member.tobuf(tarfile.GNU_FORMAT)
                # Then emit its content.
                remaining = member.size
                while remaining:
                    length = min(remaining, zstd.DECOMPRESSION_RECOMMENDED_OUTPUT_SIZE)
                    buf = reader.read(length)
                    remaining -= len(buf)
                    yield buf
                # Pad to fill a 512 bytes block, per tar format.
                remainder = member.size % 512
                if remainder:
                    yield ("\0" * (512 - remainder)).encode("utf-8")

                reader.close()

    docker.post_to_docker(download_and_modify_image(), "/images/load", quiet=0)

    # Check that we found a repositories file
    if not info.get("image") or not info.get("tag") or not info.get("layer"):
        raise Exception("No repositories file found!")

    return info
