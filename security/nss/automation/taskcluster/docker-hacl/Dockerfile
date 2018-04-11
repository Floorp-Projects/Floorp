FROM ubuntu:xenial

MAINTAINER Franziskus Kiefer <franziskuskiefer@gmail.com>
# Based on the HACL* image from Benjamin Beurdouche and
# the original F* formula with Daniel Fabian

# Pinned versions of HACL* (F* and KreMLin are pinned as submodules)
ENV haclrepo https://github.com/mitls/hacl-star.git

# Define versions of dependencies
ENV opamv 4.04.2
ENV haclversion e13326efee1a9910004dccbb56f3d7be6639e0b8

# Install required packages and set versions
ADD setup.sh /tmp/setup.sh
RUN bash /tmp/setup.sh

# Create user, add scripts.
RUN useradd -ms /bin/bash worker
WORKDIR /home/worker
ADD bin /home/worker/bin
RUN chmod +x /home/worker/bin/*
USER worker

# Build F*, HACL*, verify. Install a few more dependencies.
ENV OPAMYES true
ENV PATH "/home/worker/hacl-star/dependencies/z3/bin:$PATH"
ADD setup-user.sh /tmp/setup-user.sh
ADD license.txt /tmp/license.txt
RUN bash /tmp/setup-user.sh
