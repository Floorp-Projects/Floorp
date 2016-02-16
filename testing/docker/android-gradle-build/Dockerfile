FROM          centos:centos7
MAINTAINER    Nick Alexander <nalexander@mozilla.com>

# Reset user/workdir from parent image so we can install software.
WORKDIR /
USER root

# Update base.
RUN yum upgrade -y

# Install JDK and Sonatype Nexus.  Cribbed directly from
# https://github.com/sonatype/docker-nexus/blob/fffd2c61b2368292040910c055cf690c8e76a272/oss/Dockerfile.

RUN yum install -y \
  createrepo \
  curl \
  java-1.7.0-openjdk-devel \
  java-1.7.0-openjdk \
  sudo \
  tar \
  unzip \
  wget \
  zip \
  && yum clean all

ENV NEXUS_VERSION 2.12.0-01

RUN mkdir -p /opt/sonatype/nexus

WORKDIR /tmp
RUN curl --fail --silent --location --retry 3 \
    https://download.sonatype.com/nexus/oss/nexus-${NEXUS_VERSION}-bundle.tar.gz \
    -o /tmp/nexus-${NEXUS_VERSION}-bundle.tar.gz
RUN curl --fail --silent --location --retry 3 \
    https://download.sonatype.com/nexus/oss/nexus-${NEXUS_VERSION}-bundle.tar.gz.sha1 \
    -o /tmp/nexus-${NEXUS_VERSION}-bundle.tar.gz.sha1

RUN echo $(cat nexus-${NEXUS_VERSION}-bundle.tar.gz.sha1) nexus-${NEXUS_VERSION}-bundle.tar.gz | sha1sum -c

RUN tar zxf nexus-${NEXUS_VERSION}-bundle.tar.gz \
  && mv /tmp/nexus-${NEXUS_VERSION}/* /opt/sonatype/nexus/ \
  && rm -rf /tmp/nexus-${NEXUS_VERSION} \
  && rm -rf /tmp/nexus-${NEXUS_VERSION}-bundle.tar.gz

# Install tooltool directly from github.
RUN mkdir /build
ADD https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py /build/tooltool.py
RUN chmod +rx /build/tooltool.py

# Add build scripts.
ADD             build.sh /build/
RUN             chmod +x /build/*

ADD project /project

# Invoke our build scripts by default, but allow other commands.
CMD /build/build.sh
