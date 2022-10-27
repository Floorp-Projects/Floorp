# Dockerfile for building extra builds.  This includes more tools than the
# default image, so it's a fair bit bigger.  Only use this for builds where
# the smaller docker image is missing something.  These builds will run on
# the leaner configuration.
FROM ubuntu:18.04
LABEL maintainer="Martin Thomson <martin.thomson@gmail.com>"

RUN dpkg --add-architecture i386
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    clang-4.0 \
    clang \
    cmake \
    curl \
    g++-4.8-multilib \
    g++-5-multilib \
    g++-6-multilib \
    g++-multilib \
    git \
    gyp \
    libelf-dev \
    libdw-dev \
    libssl-dev \
    libssl-dev:i386 \
    libxml2-utils \
    lib32z1-dev \
    linux-libc-dev:i386 \
    llvm-dev \
    locales \
    mercurial \
    ninja-build \
    pkg-config \
    valgrind \
    zlib1g-dev \
    clang-format-10 \
    sqlite3 \ 
    libabigail-dev \
    abigail-tools \
    software-properties-common \ 
 && add-apt-repository ppa:ubuntu-toolchain-r/test -y \ 
 && apt-get update \
 && apt-get install --no-install-recommends -y \ 
    gcc-9 \ 
    g++-9 \ 
    gcc-9-multilib \
    g++-9-multilib \ 
    gcc-10 \
    g++-10 \ 
    gcc-10-multilib \
    g++-10-multilib \
    gcc-11-multilib \
    g++-11-multilib \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y

RUN update-alternatives --install /usr/bin/clang-format \
    clang-format $(which clang-format-10) 10

ENV SHELL /bin/bash
ENV USER worker
ENV LOGNAME $USER
ENV HOME /home/$USER
ENV LANG en_US.UTF-8
ENV LC_ALL $LANG
ENV HOST localhost
ENV DOMSUF localdomain

RUN locale-gen $LANG \
 && DEBIAN_FRONTEND=noninteractive dpkg-reconfigure locales

RUN useradd -d $HOME -s $SHELL -m $USER
WORKDIR $HOME

# Add build and test scripts.
ADD bin $HOME/bin
RUN chmod +x $HOME/bin/*

USER $USER

# Set a default command for debugging.
CMD ["/bin/bash", "--login"]
