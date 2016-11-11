# Docker Images for use in TaskCluster

This folder contains various docker images used in [taskcluster](http://docs.taskcluster.net/) as well as other misc docker images which may be useful for
hacking on gecko.

## Organization

Each folder describes a single docker image.  We have two types of images that can be defined:

1. [Task Images (build-on-push)](#task-images-build-on-push)
2. [Docker Images (prebuilt)](#docker-registry-images-prebuilt)

These images depend on one another, as described in the [`FROM`](https://docs.docker.com/v1.8/reference/builder/#from)
line at the top of the Dockerfile in each folder.

Images could either be an image intended for pushing to a docker registry, or one that is meant either
for local testing or being built as an artifact when pushed to vcs.

### Task Images (build-on-push)

Images can be uploaded as a task artifact, [indexed](#task-image-index-namespace) under
a given namespace, and used in other tasks by referencing the task ID.

Important to note, these images do not require building and pushing to a docker registry, and are
build per push (if necessary) and uploaded as task artifacts.

The decision task that is run per push will [determine](#context-directory-hashing)
if the image needs to be built based on the hash of the context directory and if the image
exists under the namespace for a given branch.

As an additional convenience, and a precaution to loading images per branch, if an image
has been indexed with a given context hash for mozilla-central, any tasks requiring that image
will use that indexed task.  This is to ensure there are not multiple images built/used
that were built from the same context. In summary, if the image has been built for mozilla-central,
pushes to any branch will use that already built image.

To use within an in-tree task definition, the format is:

```yaml
image:
  type: 'task-image'
  path: 'public/image.tar.zst'
  taskId: '{{#task_id_for_image}}builder{{/task_id_for_image}}'
```

##### Context Directory Hashing

Decision tasks will calculate the sha256 hash of the contents of the image
directory and will determine if the image already exists for a given branch and hash
or if a new image must be built and indexed.

Note: this is the contents of *only* the context directory, not the
image contents.

The decision task will:
1. Recursively collect the paths of all files within the context directory
2. Sort the filenames alphabetically to ensure the hash is consistently calculated
3. Generate a sha256 hash of the contents of each file.
4. All file hashes will then be combined with their path and used to update the hash
of the context directory.

This ensures that the hash is consistently calculated and path changes will result
in different hashes being generated.

##### Task Image Index Namespace

Images that are built on push and uploaded as an artifact of a task will be indexed under the
following namespaces.

* docker.images.v2.level-{level}.{image_name}.latest
* docker.images.v2.level-{level}.{image_name}.pushdate.{year}.{month}-{day}-{pushtime}
* docker.images.v2.level-{level}.{image_name}.hash.{context_hash}

Not only can images be browsed by the pushdate and context hash, but the 'latest' namespace
is meant to view the latest built image.  This functions similarly to the 'latest' tag
for docker images that are pushed to a registry.

### Docker Registry Images (prebuilt)

***Deprecation Warning: Use of prebuilt images should only be used for base images (those that other images
will inherit from), or private images that must be stored in a private docker registry account.  Existing
public images will be converted to images that are built on push and any newly added image should
follow this pattern.***

These are images that are intended to be pushed to a docker registry and used by specifying the
folder name in task definitions.  This information is automatically populated by using the 'docker_image'
convenience method in task definitions.

Example:
  image: {#docker_image}builder{/docker_image}

Each image has a version, given by its `VERSION` file.  This should be bumped when any changes are made that will be deployed into taskcluster.
Then, older tasks which were designed to run on an older version of the image can still be executed in taskcluster, while new tasks can use the new version.

Each image also has a `REGISTRY`, defaulting to the `REGISTRY` in this directory, and specifying the image registry to which the completed image should be uploaded.

## Building images

Generally, images can be pulled from the [registry](./REGISTRY) rather than
built locally, however, for developing new images it's often helpful to hack on
them locally.

To build an image, invoke `build.sh` with the name of the folder (without a trailing slash):
```sh
./build.sh base
```

This is a tiny wrapper around building the docker images via `docker
build -t $REGISTRY/$FOLDER:$FOLDER_VERSION`

Note: If no "VERSION" file present in the image directory, the tag 'latest' will be used and no
registry user will be defined.  The image is only meant to run locally and will overwrite
any existing image with the same name and tag.

On completion, if the image has been tagged with a version and registry, `build.sh` gives a
command to upload the image to the registry, but this is not necessary until the image
is ready for production usage. Docker will successfully find the local, tagged image
while you continue to hack on the image definitions.

## Adding a new image

The docker image primitives are very basic building block for
constructing an "image" but generally don't help much with tagging it
for deployment so we have a wrapper (./build.sh) which adds some sugar
to help with tagging/versioning... Each folder should look something
like this:

```
  - your_amazing_image/
    - your_amazing_image/Dockerfile: Standard docker file syntax
    - your_amazing_image/VERSION: The version of the docker file
      (required* used during tagging)
    - your_amazing_image/REGISTRY: Override default registry
      (useful for secret registries)
```

## Conventions

In some image folders you will see `.env` files these can be used in
conjunction with the `--env-file` flag in docker to provide a
environment with the given environment variables. These are primarily
for convenience when manually hacking on the images.

You will also see a `system-setup.sh` script used to build the image.
Do not replicate this technique - prefer to include the commands and options directly in the Dockerfile.
