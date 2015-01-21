# Docker Images for use in TaskCluster

This folder contains various docker images used in [taskcluster](http://docs.taskcluster.net/) as well as other misc docker images which may be useful for
hacking on gecko.

## Organization

Each folder describes a single docker image.
These images depend on one another, as described in the `FROM` line at the top of the Dockerfile in each folder.
Each image has a version, given by its `VERSION` file.  This should be bumped when any changes are made that will be deployed into taskcluster.
Then, older tasks which were designed to run on an older version of the image can still be executed in taskcluster, while new tasks can use the new version.

Each image also has a `REGISTRY`, defaulting to the `REGISTRY` in this directory, and specifying the image registry to which the completed image should be uploaded.

## Building images

Generally images can be pulled from the [registry](./REGISTRY) rather then
build locally, but for developing new images its often helpful to hack on
them locally.

To build an image, invoke `build.sh` with the name of the folder (without a trailing slash):
```sh
./build.sh base
```

This is a tiny wrapper around building the docker images via `docker
build -t $REGISTRY/$FOLDER:$FOLDER_VERSION`

On completion, `build.sh` gives a command to upload the image to the registry, but this is not necessary until the image is ready for production usage.
Docker will successfully find the local, tagged image while you continue to hack on the image definitions.

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
