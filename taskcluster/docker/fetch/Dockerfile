# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT

RUN apt-get update && \
    apt-get install \
      gnupg

# %include taskcluster/scripts/misc/fetch-content
ADD topsrcdir/taskcluster/scripts/misc/fetch-content /builds/worker/bin/fetch-content
