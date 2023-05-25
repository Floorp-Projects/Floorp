# Minimal image with clang-format 3.9.
FROM rust:1.64
LABEL maintainer="iaroslav.gridin@tuni.fi"

# for new clang/llvm
RUN echo "deb http://ftp.debian.org/debian/ sid main" > /etc/apt/sources.list.d/sid.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
    ca-certificates \
    locales \
    python-dev-is-python3 \
    mercurial \
    python3-pip \
    python-setuptools \
    build-essential \
    cargo \
    rustc \
    git \
    gyp \
    clang-15 \
    libclang-rt-15-dev \
    llvm-15 \
    ninja-build \
    binutils \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y

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
