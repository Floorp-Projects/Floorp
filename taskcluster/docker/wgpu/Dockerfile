FROM $DOCKER_IMAGE_PARENT
MAINTAINER Dzmitry Malyshau <dmalyshau@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ADD system-setup.sh /setup/system-setup.sh
RUN bash /setup/system-setup.sh

# We need this to install cargo-vendor as part of the wgpu-deps task
RUN apt-get install libssl-dev

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
