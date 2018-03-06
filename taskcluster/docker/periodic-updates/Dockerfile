FROM ubuntu:bionic
MAINTAINER Simon Fraser <sfraser@mozilla.com>

# Required software
ENV DEBIAN_FRONTEND noninteractive

# Chain apt-get commands with apt-get clean in a single docker RUN
# to make sure that files are removed within a single docker layer

RUN apt-get update -q && \
    apt-get install -y --no-install-recommends \
    arcanist=0~git20170812-1 \
    bzip2=1.0.6-8.1 \
    ca-certificates=20170717 \
    curl=7.58.0-2ubuntu1 \
    jq=1.5+dfsg-2 \
    libdbus-glib-1-2=0.110-2 \
    libgtk-3-0=3.22.28-1ubuntu1  \
    libxml2-utils=2.9.4+dfsg1-6.1ubuntu1 \
    libxt6=1:1.1.5-1 \
    mercurial=4.3.1-2 \
    python3=3.6.4-1 \
    shellcheck=0.4.6-1 \
    unzip=6.0-21ubuntu1 \
    wget=1.19.4-1ubuntu2 \
    && apt-get clean

RUN useradd -d /home/worker -s /bin/bash -m worker

COPY runme.sh /
COPY scripts/* /home/worker/scripts/

ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker

CMD ["/runme.sh"]
