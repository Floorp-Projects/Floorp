FROM debian:wheezy-20171210
MAINTAINER Mike Hommey <mhommey@mozilla.com>

### Add worker user and setup its workspace.
RUN mkdir /builds && \
    groupadd -g 500 worker && \
    useradd -u 500 -g 500 -d /builds/worker -s /bin/bash -m worker && \
    mkdir -p /builds/worker/workspace && \
    chown -R worker:worker /builds

# Declare default working folder
WORKDIR /builds/worker

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV HOME=/builds/worker \
    SHELL=/bin/bash \
    USER=worker \
    LOGNAME=worker \
    HOSTNAME=taskcluster-worker \
    XZ_OPT=-T0 \
    DEBIAN_FRONTEND=noninteractive

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]

# Set apt sources list to a snapshot.
RUN for s in debian_wheezy debian_wheezy-updates debian-security_wheezy/updates; do \
      echo "deb http://snapshot.debian.org/archive/${s%_*}/20171210T214726Z/ ${s#*_} main"; \
    done > /etc/apt/sources.list

RUN apt-get -o Acquire::Check-Valid-Until=false update -q && \
    apt-get install -yyq --no-install-recommends \
      apt-transport-https \
      ca-certificates

# %ARG DOCKER_IMAGE_PACKAGES
RUN for task in $DOCKER_IMAGE_PACKAGES; do \
      echo "deb [trusted=yes] https://queue.taskcluster.net/v1/task/$task/runs/0/artifacts/public/build/ debian/" >> /etc/apt/sources.list; \
    done

RUN dpkg --add-architecture i386

RUN apt-get -o Acquire::Check-Valid-Until=false update -q && \
    apt-get install -yyq --no-install-recommends \
      autoconf \
      automake \
      bison \
      build-essential \
      curl \
      cmake \
      flex \
      gawk \
      gcc-multilib \
      git \
      gnupg \
      libbz2-dev \
      libssl-dev \
      libtool \
      libx11-dev:i386 \
      make \
      mercurial \
      ninja-build \
      p7zip-full \
      procps \
      python-dev \
      python-pip \
      python-setuptools \
      python-virtualenv \
      scons \
      subversion \
      tar \
      unzip \
      uuid \
      uuid-dev \
      wget \
      xz-utils \
      zip \
      zlib1g-dev \
    && \
    apt-get clean

# %include testing/mozharness/external_tools/robustcheckout.py
COPY topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/hgrc
COPY topsrcdir/taskcluster/docker/recipes/hgrc /etc/mercurial/hgrc.d/mozilla.rc

# Add pip configuration, among other things.
# %include taskcluster/docker/recipes/dot-config
COPY topsrcdir/taskcluster/docker/recipes/dot-config /builds/worker/.config

# %include taskcluster/docker/recipes/run-task
COPY topsrcdir/taskcluster/docker/recipes/run-task /builds/worker/bin/run-task

RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/*
