# Dockerfile for running fuzzing tests on linux32.
#
# This is a temporary workaround for bugs in clang that make it incompatible
# with Ubuntu 18.04 (see bug 1488148). This image can be removed once a new
# release of LLVM includes the necessary fixes.

FROM ubuntu:16.04
LABEL maintainer="Martin Thomson <martin.thomson@gmail.com>"

RUN dpkg --add-architecture i386
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    g++-multilib \
    git \
    gyp \
    libssl-dev \
    libssl-dev:i386 \
    libxml2-utils \
    lib32z1-dev \
    linux-libc-dev:i386 \
    locales \
    mercurial \
    ninja-build \
    pkg-config \
    software-properties-common \
    valgrind \
    zlib1g-dev \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y

# Install clang and tools from the LLVM PPA.
RUN curl -sf https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - \
 && apt-add-repository "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-6.0 main" \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
    clang-6.0 \
    clang-tools-6.0 \
    llvm-6.0-dev \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y

# Alias all the clang commands.
RUN for i in $(dpkg -L clang-6.0 clang-tools-6.0 | grep '^/usr/bin/' | xargs -i basename {} -6.0); do \
      update-alternatives --install "/usr/bin/$i" "$i" "/usr/bin/${i}-6.0" 10; \
    done

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

# Change user.
USER $USER

# Set a default command for debugging.
CMD ["/bin/bash", "--login"]
