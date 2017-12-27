FROM debian:stretch-20170620
MAINTAINER Nick Alexander <nalexander@mozilla.com>

### Add worker user and setup its workspace.
RUN mkdir -p /builds && \
    groupadd -g 500 worker && \
    useradd -u 500 -g 500 -d /builds/worker -s /bin/bash -m worker && \
    chown -R worker:worker /builds && \
    mkdir -p /builds/worker/workspace && \
    chown -R worker:worker /builds/worker/workspace

# Declare default working folder
WORKDIR /builds/worker

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

# Add common hg configuration.
# %include taskcluster/docker/centos6-build/hgrc
COPY topsrcdir/taskcluster/docker/centos6-build/hgrc /builds/worker/.hgrc
RUN chown worker:worker /builds/worker/.hgrc

# Set variables normally configured at login, by the shell' parent process,
# these are taken from GNU su manual.  Also set DEBIAN_FRONTEND to advise
# apt-get of our situation.
ENV HOME=/builds/worker \
    SHELL=/bin/bash \
    USER=worker \
    LOGNAME=worker \
    HOSTNAME=taskcluster-worker \
    DEBIAN_FRONTEND=noninteractive

# Set a default command useful for debugging.
CMD ["/bin/bash", "--login"]

# Set apt sources list to a snapshot.
COPY sources.list /etc/apt/

# We need i386 packages for the Android SDK.
# Once https://bugzilla.mozilla.org/show_bug.cgi?id=1370119 is in-tree, we
# will have 64-bit builds of everything, and we can then remove this and
# the :i386 packages we install below.
RUN dpkg --add-architecture i386

# rsync is required for l10n single locale repacks.  less, screen, and
# vim, help debugging interactive tasks in Task Cluster.
RUN apt-get update -q && \
    apt-get install -yyq --no-install-recommends \
      autoconf2.13 \
      build-essential \
      base-files \
      ca-certificates \
      ccache \
      cmake \
      curl \
      file \
      gnupg \
      less \
      make \
      procps \
      python \
      python-cryptography \
      python-dev \
      python-pip \
      python-setuptools \
      python-virtualenv \
      rsync \
      screen \
      sudo \
      tar \
      unzip \
      uuid \
      vim \
      wget \
      xz-utils \
      yasm \
      zip \
      libstdc++6:i386 \
      libgcc1:i386 \
      zlib1g:i386 \
      libncurses5:i386 \
    && \
    apt-get clean

# %include python/mozbuild/mozbuild/action/tooltool.py
COPY topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /setup/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
COPY topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/common.sh
COPY topsrcdir/taskcluster/docker/recipes/common.sh /setup/common.sh

# %include taskcluster/docker/recipes/install-mercurial.sh
COPY topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /setup/install-mercurial.sh

# %include taskcluster/docker/recipes/debian-build-system-setup.sh
COPY topsrcdir/taskcluster/docker/recipes/debian-build-system-setup.sh /setup/system-setup.sh

RUN bash /setup/system-setup.sh

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb.
# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# %include taskcluster/docker/recipes/run-task
COPY topsrcdir/taskcluster/docker/recipes/run-task /builds/worker/bin/run-task

# Add pip configuration, among other things.
COPY dot-config /builds/worker/.config

# Stubbed out credentials; mozharness looks for this file an issues a WARNING
# if it's not found, which causes the build to fail.  Note that this needs to
# be in the parent of the workspace directory and in the directory where
# mozharness is run (not its --work-dir).  See Bug 1169652.
# %include taskcluster/docker/desktop-build/oauth.txt
COPY topsrcdir/taskcluster/docker/desktop-build/oauth.txt /builds/worker/

# Add a stubbed out buildprops, which keeps mozharness from choking.
# Note that this needs to be in the parent of the workspace directory and in
# the directory where mozharness is run (not its --work-dir).
# %include taskcluster/docker/desktop-build/buildprops.json
COPY topsrcdir/taskcluster/docker/desktop-build/buildprops.json /builds/worker/

# Reset user/workdir from parent image so we can install software.
WORKDIR /
USER root

# Install Sonatype Nexus.  Cribbed directly from
# https://github.com/sonatype/docker-nexus/blob/fffd2c61b2368292040910c055cf690c8e76a272/oss/Dockerfile.

# Observe missing --no-install-recommends, in order to install glib2.0/gconf/etc.
RUN apt-get install -yyq \
      libgconf2-4 \
      libgnome2-0 \
      openjdk-8-jdk-headless \
    && \
    apt-get clean

ENV NEXUS_VERSION=2.12.0-01 \
    NEXUS_SHA1SUM=1a9aaad8414baffe0a2fd46eed1f41b85f4049e6 \
    NEXUS_WORK=/builds/worker/workspace/nexus

RUN mkdir -p /opt/sonatype/nexus

WORKDIR /tmp
RUN curl --fail --silent --location --retry 3 \
    https://download.sonatype.com/nexus/oss/nexus-${NEXUS_VERSION}-bundle.tar.gz \
    -o /tmp/nexus-${NEXUS_VERSION}-bundle.tar.gz

# Observe the two spaces below.  Seriously.
RUN echo "${NEXUS_SHA1SUM}  nexus-${NEXUS_VERSION}-bundle.tar.gz" > nexus-${NEXUS_VERSION}-bundle.tar.gz.sha1
RUN sha1sum --check nexus-${NEXUS_VERSION}-bundle.tar.gz.sha1

RUN tar zxf nexus-${NEXUS_VERSION}-bundle.tar.gz \
  && mv /tmp/nexus-${NEXUS_VERSION}/* /opt/sonatype/nexus/ \
  && rm -rf /tmp/nexus-${NEXUS_VERSION} \
  && rm -rf /tmp/nexus-${NEXUS_VERSION}-bundle.tar.gz

# So that we don't have to RUN_AS_USER=root.
RUN chown -R worker:worker /opt/sonatype

# Back to the workdir, matching desktop-build.
WORKDIR /builds/worker
