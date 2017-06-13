FROM ubuntu:16.04
MAINTAINER Franziskus Kiefer <franziskuskiefer@gmail.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

# Install dependencies.
ADD setup.sh /tmp/setup.sh
RUN bash /tmp/setup.sh

# Change user.
USER worker

# Env variables.
ENV HOME /home/worker
ENV SHELL /bin/bash
ENV USER worker
ENV LOGNAME worker
ENV HOSTNAME taskcluster-worker
ENV LANG en_US.UTF-8
ENV LC_ALL en_US.UTF-8
ENV HOST localhost
ENV DOMSUF localdomain

# Entrypoint.
ENTRYPOINT ["/home/worker/nss/automation/clang-format/run_clang_format.sh"]
