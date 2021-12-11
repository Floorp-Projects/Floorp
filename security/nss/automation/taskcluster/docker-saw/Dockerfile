FROM ubuntu:16.04
MAINTAINER Tim Taubert <ttaubert@mozilla.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y \
    binutils \
    build-essential \
    bzip2 \
    clang-3.8 \
    curl \
    gcc-multilib \
    g++-multilib \
    gyp \
    lib32z1-dev \
    mercurial \
    ninja-build \
    unzip \
    zlib1g-dev

# Add missing LLVM plugin for gold linker.
ADD LLVMgold.so.zip /usr/lib/llvm-3.8/lib/LLVMgold.so.zip
RUN unzip /usr/lib/llvm-3.8/lib/LLVMgold.so.zip -d /usr/lib/llvm-3.8/lib/

# Install SAW/Cryptol.
RUN curl -LO https://saw.galois.com/builds/nightly/saw-0.2-2018-01-14-Ubuntu14.04-64.tar.gz && \
    tar xzvf saw-*.tar.gz -C /usr/local --strip-components=1 && \
    rm saw-*.tar.gz

# Install Z3.
RUN curl -LO https://github.com/Z3Prover/z3/releases/download/z3-4.6.0/z3-4.6.0-x64-ubuntu-16.04.zip && \
    unzip z3*.zip && \
    cp -r z3*/* /usr/local/ && \
    rm -fr z3*

ADD bin /home/worker/bin
RUN chmod +x /home/worker/bin/*

# Change user.
USER worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
