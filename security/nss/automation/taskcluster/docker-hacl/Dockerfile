FROM ubuntu:xenial

MAINTAINER Franziskus Kiefer <franziskuskiefer@gmail.com>
# Based on the HACL* image from Benjamin Beurdouche and
# the original F* formula with Daniel Fabian

# Pinned versions of HACL* (F* and KreMLin are pinned as submodules)
ENV haclrepo https://github.com/mitls/hacl-star.git

# Define versions of dependencies
ENV opamv 4.05.0
ENV haclversion 1442c015dab97cdf203ae238b1f3aeccf511bd1e

# Install required packages and set versions
ADD B6C8F98282B944E3B0D5C2530FC3042E345AD05D.asc /tmp/B6C8F98282B944E3B0D5C2530FC3042E345AD05D.asc
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
