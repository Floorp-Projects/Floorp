FROM ubuntu:18.04
MAINTAINER Simon Fraser <sfraser@mozilla.com>

# Required software
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update -q && \
    apt-get install -yyq --no-install-recommends \
    bzip2 \
    ca-certificates \
    curl \
    gcc \
    jq \
    libdpkg-perl \
    libgetopt-simple-perl \
    liblzma-dev \
    locales \
    python3.8 \
    libpython3.8-dev \
    python3-dev \
    xz-utils
RUN useradd -d /home/worker -s /bin/bash -m worker
COPY requirements.txt /

RUN locale-gen en_CA.UTF-8
ENV LANG en_CA.UTF-8
ENV LANGUAGE en_CA.UTF-8
ENV LANG_ALL en_CA.UTF-8
ENV LC_ALL en_CA.UTF-8

# python-pip installs a lot of dependencies increasing the size of an image
# drastically. Install it like this saves us almost 200M.
RUN bash -c "curl -L https://bootstrap.pypa.io/get-pip.py | python3.8"

RUN ["pip", "install", "-r", "/requirements.txt"]

# scripts
RUN mkdir /home/worker/bin
COPY scripts/* /home/worker/bin/

COPY runme.sh /runme.sh
RUN chmod 755 /home/worker/bin/* /*.sh
RUN mkdir /home/worker/keys
COPY *.pubkey /home/worker/keys/

ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker

USER worker

CMD ["/runme.sh"]
