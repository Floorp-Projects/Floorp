FROM $DOCKER_IMAGE_PARENT

### Add worker user and setup its workspace.
RUN mkdir /builds && \
    groupadd -g 1000 worker && \
    useradd -u 1000 -g 1000 -d /builds/worker -s /bin/bash -m worker && \
    mkdir -p /builds/worker/workspace && \
    chown -R worker:worker /builds

# Declare default working folder
WORKDIR /builds/worker

ARG TASKCLUSTER_ROOT_URL
ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $TASKCLUSTER_ROOT_URL $DOCKER_IMAGE_PACKAGES && \
    apt-get update && \
    apt-get install \
      gnupg \
      bzip2 \
      git \
      openssh-client \
      python3-pip \
      python3-requests \
      python3-zstandard \
      unzip

# %include taskcluster/scripts/run-task
ADD topsrcdir/taskcluster/scripts/run-task /builds/worker/bin/run-task

# %include taskcluster/scripts/misc/fetch-content
ADD topsrcdir/taskcluster/scripts/misc/fetch-content /builds/worker/bin/fetch-content

# %include taskcluster/scripts/misc/fetch-chromium.py
ADD topsrcdir/taskcluster/scripts/misc/fetch-chromium.py /builds/worker/bin/fetch-chromium.py

RUN pip3 install redo==2.0.4 --break-system-packages
