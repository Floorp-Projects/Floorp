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
    DEBIAN_FRONTEND=noninteractive

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]

# Set apt sources list to a snapshot.
RUN for s in debian_wheezy debian_wheezy-updates debian_wheezy-backports debian-security_wheezy/updates; do \
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

# %ARG ARCH
RUN dpkg --add-architecture $ARCH

# Ideally, we wouldn't need gcc-multilib and the extra linux-libc-dev,
# but the latter is required to make the former installable, and the former
# because of bug 1409276.
RUN apt-get -o Acquire::Check-Valid-Until=false update -q && \
    apt-get install -yyq --no-install-recommends \
      autoconf2.13 \
      automake \
      bzip2 \
      curl \
      file \
      gawk \
      gcc-multilib \
      git \
      gnupg \
      make \
      mercurial \
      p7zip-full \
      procps \
      python \
      python-pip \
      python-setuptools \
      python-virtualenv \
      rsync \
      screen \
      tar \
      unzip \
      uuid \
      valgrind \
      wget \
      x11-utils \
      xvfb \
      xz-utils \
      yasm/wheezy-backports \
      zip \
      linux-libc-dev/wheezy-backports \
      linux-libc-dev:$ARCH/wheezy-backports \
      pkg-config:$ARCH \
      libdbus-glib-1-dev:$ARCH \
      libfontconfig1-dev:$ARCH \
      libfreetype6-dev:$ARCH \
      libgconf2-dev:$ARCH \
      libgtk-3-dev:$ARCH \
      libgtk2.0-dev:$ARCH \
      libpango1.0-dev:$ARCH \
      libpulse-dev:$ARCH \
      libx11-xcb-dev:$ARCH \
      libxss-dev:$ARCH \
      libxt-dev:$ARCH \
    && \
    apt-get clean

# %include testing/mozharness/external_tools/robustcheckout.py
COPY topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/hgrc
COPY topsrcdir/taskcluster/docker/recipes/hgrc /etc/mercurial/hgrc.d/mozilla.rc

# Add pip configuration, among other things.
# %include taskcluster/docker/recipes/dot-config
COPY topsrcdir/taskcluster/docker/recipes/dot-config /builds/worker/.config

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# %include taskcluster/docker/recipes/run-task
COPY topsrcdir/taskcluster/docker/recipes/run-task /builds/worker/bin/run-task

# Stubbed out credentials; mozharness looks for this file an issues a WARNING
# if it's not found, which causes the build to fail.  Note that this needs to
# be in the parent of the workspace directory and in the directory where
# mozharness is run (not its --work-dir).  See Bug 1169652.
# %include taskcluster/docker/desktop-build/oauth.txt
COPY topsrcdir/taskcluster/docker/desktop-build/oauth.txt /builds/worker/

# stubbed out buildprops, which keeps mozharness from choking
# Note that this needs to be in the parent of the workspace directory and in
# the directory where mozharness is run (not its --work-dir)
# %include taskcluster/docker/desktop-build/buildprops.json
COPY topsrcdir/taskcluster/docker/desktop-build/buildprops.json /builds/worker/

RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/*
