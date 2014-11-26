# Docker Images for Gecko

This folder contains various docker images used in [taskcluster](http://docs.taskcluster.net/) as well as other misc docker images which may be useful for
hacking on gecko.

## Building images

Generally images can pull from the [registry](./REGISTRY) rather then
build locally but for developing new images its often helpful to hack on
them locally.

```sh
# Example: ./build.sh base
./build.sh <FOLDER>
```

This is a tiny wrapper around building the docker images via `docker
build -t $REGISTRY/$FOLDER:$FOLDER_VERSION`

## Adding a new image

The docker image primitives are very basic building block for
constructing an "image" but generally don't help much with tagging it
for deployment so we have a wrapper (./build.sh) which adds some sugar
to help with tagging/versioning... Each folder should look something
like this:

  - your_amazing_image/
    - your_amazing_image/Dockerfile: Standard docker file syntax
    - your_amazing_image/VERSION: The version of the docker file
      (required* used during tagging)
    - your_amazing_image/REGISTRY: Override default registry
      (useful for secret registries)
