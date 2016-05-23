FROM          quay.io/mozilla/b2g-build:0.2.11
MAINTAINER    Dustin J. Mitchell <dustin@mozilla.com>

ENV VERSION 1.2
ENV PYTHONPATH /tools/tools/lib/python:$PYTHONPATH
ENV TOOLTOOL_CACHE  /home/worker/tools/tooltool-cache

ADD https://raw.githubusercontent.com/taskcluster/buildbot-step/19219c470bd95b390ed0b31d4bf455169bf12fca/buildbot_step.js /home/worker/bin/buildbot_step

# Add utilities and configuration
RUN mkdir -p /home/worker/bin /home/worker/tools
# Add bin tools last as they are most likely to change
RUN chown -R worker:worker /home/worker/* /home/worker/.*

# Instal build tools
RUN hg clone http://hg.mozilla.org/build/tools/ /tools/tools && \
      cd /tools/tools && \
      python setup.py install

# Initialize git (makes repo happy)
RUN git config --global user.email "mozilla@example.com" && \
    git config --global user.name "mozilla"

# VCS Tools
RUN npm install -g taskcluster-vcs@2.3.17

# TODO enable worker
# TODO volume mount permissions will be an issue
# USER worker

COPY bin /home/worker/bin
RUN chmod a+x /home/worker/bin/*
