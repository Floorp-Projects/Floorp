# This docker image is used to build the Python interpreter. It requires an old
# glibc to be compatible with all the other images with ship.
FROM          $DOCKER_IMAGE_PARENT
MAINTAINER    Serge Guelton <sguelton@mozilla.com>

# We need to declare all potentially cache volumes as caches. Also,
# making high I/O paths volumes increase I/O throughput because of
# AUFS slowness.
VOLUME  /builds/worker/.cache
VOLUME  /builds/worker/checkouts
VOLUME  /builds/worker/workspace

RUN apt-get install build-essential \
                    libexpat1-dev \
                    libffi-dev \
                    liblzma-dev \
                    libncursesw5-dev \
                    libsqlite3-dev \
                    libssl-dev \
                    libbz2-dev

CMD ["/bin/bash", "--login"]
