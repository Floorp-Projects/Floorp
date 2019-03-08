# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT

RUN apt-get update && \
    apt-get install \
      gnupg \
      bzip2 \
      python3-requests \
      unzip

# %include taskcluster/scripts/misc/fetch-content
ADD topsrcdir/taskcluster/scripts/misc/fetch-content /builds/worker/bin/fetch-content

# %include taskcluster/scripts/misc/fetch-chromium.py
ADD topsrcdir/taskcluster/scripts/misc/fetch-chromium.py /builds/worker/bin/fetch-chromium.py
