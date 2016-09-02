# TODO remove VOLUME below when the base image is updated next.
FROM          taskcluster/centos6-build-upd:0.1.6.20160329195300
MAINTAINER    Nick Alexander <nalexander@mozilla.com>

# BEGIN ../desktop-build/Dockerfile

# TODO remove when base image is updated
VOLUME /home/worker/workspace
VOLUME /home/worker/tooltool-cache

# Add build scripts; these are the entry points from the taskcluster worker, and
# operate on environment variables
ADD             bin /home/worker/bin
RUN             chmod +x /home/worker/bin/*

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include testing/docker/recipes/xvfb.sh
ADD topsrcdir/testing/docker/recipes/xvfb.sh /home/worker/scripts/xvfb.sh

# Add configuration
COPY            dot-config                    /home/worker/.config

# Generate machine uuid file
RUN dbus-uuidgen --ensure=/var/lib/dbus/machine-id

# Stubbed out credentials; mozharness looks for this file an issues a WARNING
# if it's not found, which causes the build to fail.  Note that this needs to
# be in the parent of the workspace directory and in the directory where
# mozharness is run (not its --work-dir).  See Bug 1169652.
ADD           oauth.txt /home/worker/

# stubbed out buildprops, which keeps mozharness from choking
# Note that this needs to be in the parent of the workspace directory and in
# the directory where mozharness is run (not its --work-dir)
ADD           buildprops.json /home/worker/

# install tooltool directly from github where tooltool_wrapper.sh et al. expect
# to find it
RUN wget -O /builds/tooltool.py https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py
RUN chmod +x /builds/tooltool.py

# END ../desktop-build/Dockerfile

# Reset user/workdir from parent image so we can install software.
WORKDIR /
USER root

# Update base.
RUN yum upgrade -y

# Install JDK and Sonatype Nexus.  Cribbed directly from
# https://github.com/sonatype/docker-nexus/blob/fffd2c61b2368292040910c055cf690c8e76a272/oss/Dockerfile.

# Install the screen package here to use with xvfb.
# Move installation to base centos6-build image once Bug 1272629 is fixed
RUN yum install -y \
  createrepo \
  curl \
  java-1.7.0-openjdk-devel \
  java-1.7.0-openjdk \
  screen \
  sudo \
  tar \
  unzip \
  wget \
  zip \
  && yum clean all

ENV NEXUS_VERSION 2.12.0-01
ENV NEXUS_SHA1SUM 1a9aaad8414baffe0a2fd46eed1f41b85f4049e6

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

# Install tooltool directly from github.
RUN mkdir /build
ADD https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py /build/tooltool.py
RUN chmod +rx /build/tooltool.py

# Back to the centos6-build workdir, matching desktop-build.
WORKDIR /home/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
