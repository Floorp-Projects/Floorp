# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Kartikaya Gupta <kgupta@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

# %include gfx/wr/ci-scripts/docker-image/setup.sh
ADD topsrcdir/gfx/wr/ci-scripts/docker-image/setup.sh /tmp/wr-setup.sh
RUN /bin/bash /tmp/wr-setup.sh && rm /tmp/wr-setup.sh

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
