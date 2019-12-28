# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Kartikaya Gupta <kgupta@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache
VOLUME /builds/worker/.ghsync

# %include taskcluster/docker/github-sync/prepare.sh
ADD topsrcdir/taskcluster/docker/github-sync/prepare.sh /setup/prepare-docker.sh
# %include taskcluster/docker/github-sync/requirements.txt
ADD topsrcdir/taskcluster/docker/github-sync/requirements.txt /setup/requirements.txt
RUN /bin/bash /setup/prepare-docker.sh /setup/requirements.txt && rm -R /setup

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
