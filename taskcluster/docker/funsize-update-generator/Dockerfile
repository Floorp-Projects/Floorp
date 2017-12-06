FROM ubuntu:vivid
MAINTAINER Rail Aliiev <rail@mozilla.com>

# Required software
ENV DEBIAN_FRONTEND noninteractive
# Ubuntu Vivid has been moved to the old-releases repo
RUN sed -i -e 's/archive.ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list
# Chain apt-get commands with apt-get clean in a single docker RUN
# to make sure that files are removed within a single docker layer
RUN apt-get update -q && \
    apt-get install -yyq --no-install-recommends \
    python python-setuptools python-cryptography  libgetopt-simple-perl \
    bzip2 clamav clamav-freshclam python-requests python-sh curl \
    python-dev gcc liblzma-dev xz-utils && \
    apt-get clean
RUN useradd -d /home/worker -s /bin/bash -m worker
COPY requirements.txt /tmp/

# Freshclam may be flaky, retry if it fails
RUN for i in 1 2 3 4 5; do freshclam --verbose && break || sleep 15; done

# python-pip installs a lot of dependencies increasing the size of an image
# drastically. Using easy_install saves us almost 200M.
RUN easy_install pip
RUN pip install -r /tmp/requirements.txt

# scripts
RUN mkdir /home/worker/bin
COPY scripts/* /home/worker/bin/

COPY runme.sh /runme.sh
COPY recompress.sh /recompress.sh
RUN chmod 755 /home/worker/bin/* /*.sh
RUN mkdir /home/worker/keys
COPY *.pubkey /home/worker/keys/

ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker

CMD ["/runme.sh"]
