FROM          quay.io/mozilla/b2g-build:0.2.5
MAINTAINER    Dustin J. Mitchell <dustin@mozilla.com>

ENV PYTHONPATH /tools/tools/lib/python:$PYTHONPATH
ENV TOOLTOOL_CACHE  /home/worker/tools/tooltool-cache

# Add utilities and configuration
RUN mkdir -p /home/worker/bin /home/worker/tools
RUN chown -R worker:worker /home/worker/* /home/worker/.*

# Install android repo tool
RUN curl https://storage.googleapis.com/git-repo-downloads/repo > /home/worker/bin/repo
RUN chmod a+x /home/worker/bin/repo

# Instal build tools
RUN hg clone http://hg.mozilla.org/build/tools/ /tools/tools && \
      cd /tools/tools && \
      python setup.py install


# Initialize git (makes repo happy)
RUN git config --global user.email "docker@docker.com" && \
    git config --global user.name "docker"

# VCS Tools
RUN npm install -g taskcluster-vcs@2.2.0

# Add bin tools last as they are most likely to change
COPY bin /home/worker/bin

# TODO enable worker
# TODO volume mount permissions will be an issue
# USER worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
