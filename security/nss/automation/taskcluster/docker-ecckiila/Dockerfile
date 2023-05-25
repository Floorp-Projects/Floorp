# Minimal image with clang-format 3.9.
FROM ubuntu:bionic-20221215
LABEL maintainer="iaroslav.gridin@tuni.fi"

# for new clang/llvm
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
  ca-certificates \
  locales \
  cmake \
  build-essential \
  git \
  clang-10 \
  mercurial \
  unifdef \
  python3-mako \
  libgmp-dev \
  jq \
  python3-setuptools \
  python3-pip \
  python3-dev \
  clang-format-10 \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y

RUN pip3 install fastecdsa

ENV SHELL /bin/bash
ENV USER worker
ENV LOGNAME $USER
ENV HOME /home/$USER
ENV HOSTNAME taskcluster-worker
ENV LANG en_US.UTF-8
ENV LC_ALL $LANG
ENV HOST localhost
ENV DOMSUF localdomain

RUN locale-gen $LANG \
 && DEBIAN_FRONTEND=noninteractive dpkg-reconfigure locales

RUN useradd -d $HOME -s $SHELL -m $USER
WORKDIR $HOME

ADD bin $HOME/bin
RUN chmod +x $HOME/bin/*

USER $USER

# Set a default command for debugging.
CMD ["/bin/bash", "--login"]
