FROM        ubuntu:14.04
MAINTAINER  Greg Arndt <garndt@mozilla.com>

RUN mkdir -p /home/worker/upload/logs

WORKDIR /home/worker

RUN apt-get install -y curl

# Add PPA for latest nodejs versions. Do not need to run apt-get update after this
# as the script already does it.
RUN curl -sL https://deb.nodesource.com/setup | sudo bash -

RUN apt-get upgrade -y && apt-get install -y \
  build-essential \
  ca-certificates \
  nodejs \
  python-dev \
  mercurial \
  git \
  android-tools-adb \
  android-tools-fastboot\
  jq

# Get pip and virtualenv
RUN curl https://bootstrap.pypa.io/get-pip.py | python
RUN pip install virtualenv

RUN git config --global user.email "mozilla@example.com" && \
    git config --global user.name "mozilla"


# Get node packages
RUN npm install -g taskcluster-vcs@2.3.12

WORKDIR /home/worker

ADD bin /home/worker/bin
ADD data /home/worker/data
ADD https://raw.githubusercontent.com/taskcluster/buildbot-step/master/buildbot_step /home/worker/bin/buildbot_step
RUN chmod u+x /home/worker/bin/*

ENV HOME /home/worker
ENV SHELL /bin/bash
ENV PATH $PATH:/home/worker/bin
ENV CLOUD_HOST testdroid

ENTRYPOINT ["entrypoint"]
