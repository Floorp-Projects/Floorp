FROM          quay.io/mozilla/b2g-build:0.2.9
MAINTAINER    Dustin J. Mitchell <dustin@mozilla.com>

ENV PYTHONPATH /tools/tools/lib/python:$PYTHONPATH
ENV TOOLTOOL_CACHE  /home/worker/tools/tooltool-cache

ADD https://raw.githubusercontent.com/taskcluster/buildbot-step/58a16f7370a8b4de7a4458436a4a5fad9905f5d9/buildbot_step.js /home/worker/bin/buildbot_step

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
RUN npm install -g taskcluster-vcs@2.3.12

# TODO enable worker
# TODO volume mount permissions will be an issue
# USER worker

COPY bin /home/worker/bin
RUN chmod a+x /home/worker/bin/*
