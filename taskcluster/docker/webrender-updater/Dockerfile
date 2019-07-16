# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Kartikaya Gupta <kgupta@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache
VOLUME /builds/worker/.wrupdater

# %include gfx/wr/ci-scripts/wrupdater/docker-setup.sh
ADD topsrcdir/gfx/wr/ci-scripts/wrupdater/docker-setup.sh /tmp/wrup-setup.sh
# %include gfx/wr/ci-scripts/wrupdater/requirements.txt
ADD topsrcdir/gfx/wr/ci-scripts/wrupdater/requirements.txt /tmp/requirements.txt
RUN /bin/bash /tmp/wrup-setup.sh && rm /tmp/wrup-setup.sh

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
