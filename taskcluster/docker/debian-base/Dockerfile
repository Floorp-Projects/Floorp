# %ARG DIST
# %ARG BASE_TAG
FROM debian:$DIST-$BASE_TAG
MAINTAINER Mike Hommey <mhommey@mozilla.com>

### Add worker user and setup its workspace.
RUN mkdir /builds && \
    groupadd -g 1000 worker && \
    useradd -u 1000 -g 1000 -d /builds/worker -s /bin/bash -m worker && \
    mkdir -p /builds/worker/workspace && \
    chown -R worker:worker /builds

# Declare default working folder
WORKDIR /builds/worker

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV HOME=/builds/worker \
    SHELL=/bin/bash \
    USER=worker \
    LOGNAME=worker \
    HOSTNAME=taskcluster-worker \
    DEBIAN_FRONTEND=noninteractive

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]

# %ARG SNAPSHOT
# Set apt sources list to a snapshot.
RUN for s in debian_$DIST debian_$DIST-updates debian_$DIST-backports debian-security_$DIST/updates; do \
      echo "deb http://snapshot.debian.org/archive/${s%_*}/$SNAPSHOT/ ${s#*_} main"; \
    done > /etc/apt/sources.list && \
    ( echo 'quiet "true";'; \
      echo 'APT::Get::Assume-Yes "true";'; \
      echo 'APT::Install-Recommends "false";'; \
      echo 'Acquire::Check-Valid-Until "false";'; \
      echo 'Acquire::Retries "5";'; \
    ) > /etc/apt/apt.conf.d/99taskcluster

RUN apt-get update && \
    apt-get install \
      apt-transport-https \
      ca-certificates

COPY setup_packages.sh /usr/local/sbin/
COPY cloud-mirror-workaround.sh /usr/local/sbin/

# %ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $DOCKER_IMAGE_PACKAGES && \
    echo 'dir::bin::methods::https "/usr/local/sbin/cloud-mirror-workaround.sh";' > /etc/apt/apt.conf.d/99cloud-mirror-workaround && \
    apt-get update && \
    apt-get install \
      git \
      make \
      mercurial \
      python \
      python3.5 \
      python3-minimal \
      python-zstandard \
      python3-zstandard \
      xz-utils

# %include testing/mozharness/external_tools/robustcheckout.py
COPY topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/hgrc
COPY topsrcdir/taskcluster/docker/recipes/hgrc /etc/mercurial/hgrc.d/mozilla.rc

# Add pip configuration, among other things.
# %include taskcluster/docker/recipes/dot-config
COPY topsrcdir/taskcluster/docker/recipes/dot-config /builds/worker/.config

# %include taskcluster/scripts/run-task
COPY topsrcdir/taskcluster/scripts/run-task /builds/worker/bin/run-task

RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/*
