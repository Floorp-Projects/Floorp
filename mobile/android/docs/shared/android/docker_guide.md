# mozilla-mobile Docker Guide
TaskCluster runs our automation tasks, such as compilation and unit tests, in a [Docker container](https://en.wikipedia.org/wiki/Docker_(software)). We maintain our own Docker images. A complete up-to-date listing is available [on Docker Hub](https://hub.docker.com/r/mozillamobile/):
- [`mozillamobile/focus-android`](https://hub.docker.com/r/mozillamobile/focus-android/)
- [`mozillamobile/firefox-tv`](https://hub.docker.com/r/mozillamobile/firefox-tv/)
- [`mozillamobile/android-components`](https://hub.docker.com/r/mozillamobile/android-components/)
- [`mozillamobile/fenix`](https://hub.docker.com/r/mozillamobile/fenix/)

 These images contain all the tools (e.g. Android SDK, Python runtime) needed to run our tasks. These images are built from a Dockerfile located in our repository (e.g. [focus-android Dockerfile](https://github.com/mozilla-mobile/focus-android/blob/master/tools/docker/Dockerfile)). Taskcluster downloads this image from [Docker Hub](https://hub.docker.com/) whenever it needs to run one of our tasks.

If you want to build or use an image locally then you need to install the Community Edition of Docker on your machine:
https://www.docker.com/community-edition

## Updating the Docker image
If our toolchain changes or if we require new tools for running tasks then it might be needed to update the image taskcluster uses. This sample will use the `mozillamobile/focus-android` docker image as an example:

1. Install Docker Community Edition on your machine

2. Modify [Dockerfile](https://github.com/mozilla-mobile/focus-android/blob/master/tools/docker/Dockerfile). See [Dockerfile reference](https://docs.docker.com/engine/reference/builder/) for a list of commands that can be used to build an image.

3. Build the Dockerfile locally and tag it _mozillamobile/focus-android_:

```bash
docker build -t mozillamobile/focus-android .
```

4. Run the image locally and test that everything is working as you intended to:

```bash
docker run -it mozillamobile/focus-android /bin/bash
```

5. Commit Dockerfile and open a PR for it.

6. Push the image to Docker Hub so that taskcluster can find it:

```bash
docker push mozillamobile/focus-android.
```

Running this command requires that you have an account, are added to the "mozillamobile" organization, and have logged in via the command line: `docker login`. Ask :sebastian if you need to be added.

Once the docker image is pushed, make sure your `taskcluster.yml` is referencing the new docker image with the correct tag number under `payload`.

Note: If there is a failure while building the docker image, this often comes from lack of memory.  Try increasing the memory and swap space on GUI and/or `-m 5g` build argument.

## Debugging errors
If you receive an error on TaskCluster that you can't reproduce locally, you may need to use Docker locally to reproduce the TaskCluster build environment.

tldr version to use docker:
```sh
# Install docker from https://www.docker.com/community-edition

# Pull image
docker pull mozillamobile/focus-android

# Start image and run bash
docker run -it mozillamobile/focus-android /bin/bash

# You now have a shell in the image with a checkout of the project (old)
# and all SDK tools installed. Update the checkout and run the failing command.
# This is an ubuntu image. Use "apt-get" to install whatever helps you (e.g. vim)
```

## Cheat sheet
A list of docker commands that can be helpful:

```sh
# See all locally available images
docker images

# Run and image and execute bash in it:
docker run -it <image> /bin/bash

# Show all containers
docker ps -a

# Start a stopped/exited container
docker start <containerId>

# Join (running) container
docker attach <containerId>

# Copy file from container to host
docker cp <containerId>:/file/path/within/container /host/path/target

# Build image from Dockerfile with tag
docker build -t <tag> .

# Push image to Docker hub
docker push <tag>

# Pull image from Docker hub
docker pull <tag>
```

## Help. I'm running out of disk space
Over time Docker is using a lot of disk space for all the images and containers you have started. At least on MacOS even removing images and containers doesn't reclaim disk space. The only thing that seems to help is to remove this gigantic Docker.qcow2 file. Note that doing that will remove all your local images and containers. You will need to re-pull or re-build images you need.

```bash
rm /Users/<your username>/Library/Containers/com.docker.docker/Data/com.docker.driver.amd64-linux/Docker.qcow2
```

More about this in [this Docker bug report](https://github.com/docker/for-mac/issues/371).
