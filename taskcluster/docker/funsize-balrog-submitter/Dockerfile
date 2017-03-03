FROM ubuntu:vivid
MAINTAINER Rail Aliiev <rail@mozilla.com>

# Required software
ENV DEBIAN_FRONTEND noninteractive
# Chain apt-get commands with apt-get clean in a single docker RUN
# to make sure that files are removed within a single docker layer
RUN apt-get update -q && \
    apt-get install -yyq --no-install-recommends \
    python mercurial curl python-boto python-setuptools python-cryptography && \
    apt-get clean

COPY requirements.txt /tmp/
# python-pip installs a lot of dependencies increasing the size of an image
# drastically.
RUN easy_install pip
RUN pip install -r /tmp/requirements.txt

RUN hg clone https://hg.mozilla.org/build/tools /home/worker/tools

RUN useradd -d /home/worker -s /bin/bash -m worker

RUN mkdir /home/worker/bin
COPY scripts/* /home/worker/bin/
RUN mkdir /home/worker/keys
COPY *.pubkey /home/worker/keys/
COPY runme.sh /runme.sh
RUN chmod 755 /home/worker/bin/* /runme.sh

ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
