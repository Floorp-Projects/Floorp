FROM ubuntu:16.04
MAINTAINER Tim Taubert <ttaubert@mozilla.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

# Install non-build specific dependencies.
ADD setup.sh /tmp/setup.sh
RUN bash /tmp/setup.sh

# Add build and test scripts.
ADD bin /home/worker/bin
RUN chmod +x /home/worker/bin/*

# Set variables usually configured at login.
ENV HOME /home/worker
ENV SHELL /bin/bash
ENV USER worker
ENV LOGNAME worker
ENV HOSTNAME taskcluster-worker
ENV LANG en_US.UTF-8
ENV LC_ALL en_US.UTF-8
env HOST localhost
env DOMSUF localdomain

# Set a default command for debugging.
CMD ["/bin/bash", "--login"]
