# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT

### Add worker user and setup its workspace.
RUN mkdir /builds && \
    groupadd -g 1000 worker && \
    useradd -u 1000 -g 1000 -d /builds/worker -s /bin/bash -m worker && \
    mkdir -p /builds/worker/workspace && \
    chown -R worker:worker /builds

# Declare default working folder
WORKDIR /builds/worker

RUN apt-get update && \
    apt-get install \
      gnupg \
      bzip2 \
      python3-requests \
      unzip

# %include taskcluster/scripts/run-task
ADD topsrcdir/taskcluster/scripts/run-task /builds/worker/bin/run-task

# %include taskcluster/scripts/misc/fetch-content
ADD topsrcdir/taskcluster/scripts/misc/fetch-content /builds/worker/bin/fetch-content

# %include taskcluster/scripts/misc/fetch-chromium.py
ADD topsrcdir/taskcluster/scripts/misc/fetch-chromium.py /builds/worker/bin/fetch-chromium.py
