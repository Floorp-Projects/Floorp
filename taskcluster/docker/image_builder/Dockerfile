# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FROM golang:1.14 as skopeo

WORKDIR /go/src/
RUN ["git", "clone", "--no-checkout", "--depth=1", "--branch=v1.1.1", "https://github.com/containers/skopeo", "."]
RUN ["git", "checkout", "67abbb3cefbdc876447583d5ea45e76bf441eba7"]
ENV GO111MODULE=on CGO_ENABLED=0
RUN ["go", "build", \
	"-mod=vendor", "-o", "out/skopeo", \
	"-tags", "exclude_graphdriver_devicemapper exclude_graphdriver_btrfs containers_image_openpgp", \
	# Set unixTempDirForBigFiles so skopeo will extract in a directory hidden by kaniko
	# We create the directory below.
	"-ldflags", " -X github.com/containers/image/v5/internal/tmpdir.unixTempDirForBigFiles=/workspace/tmp -X github.com/containers/image/v5/signature.systemDefaultPolicyPath=/kaniko/containers/policy.json -extldflags \"-static\" -w -s", \
	"./cmd/skopeo"]

FROM golang:1.14 as kaniko
WORKDIR /go/src/
RUN ["git", "clone", "--no-checkout", "--depth=1", "--branch=v1.0.0", "https://github.com/GoogleContainerTools/kaniko", "."]
RUN ["git", "checkout", "146ec6a9cd6f87b4a12e8119ded575d5edca35ac"]
RUN ["make"]

# Build the `build-image` command as a static binary using musl
# The setup is loosely based on a stripped down version of
# https://github.com/emk/rust-musl-builder/blob/master/Dockerfile
FROM debian:buster as build-image

COPY apt.conf /etc/apt/apt.conf.d/99taskcluster

RUN apt-get update && \
    apt-get install \
        build-essential \
        ca-certificates \
        curl \
        musl-dev \
        musl-tools \
        && \
    useradd rust --user-group --create-home --shell /bin/bash

# Run all further code as user `rust`, and create our working directories
# as the appropriate user.
USER rust

# Set up our path with all our binary directories, including those for the
# musl-gcc toolchain and for our Rust toolchain.
ENV PATH=/home/rust/.cargo/bin:$PATH

# The Rust toolchain to use when building our image.  Set by `hooks/build`.
ENV TOOLCHAIN=1.42.0 \
    TARGET=x86_64-unknown-linux-musl

# Install our Rust toolchain and the `musl` target.  We patch the
# command-line we pass to the installer so that it won't attempt to
# interact with the user or fool around with TTYs.  We also set the default
# `--target` to musl so that our users don't need to keep overriding it
# manually.
RUN curl https://sh.rustup.rs -sSf | \
    sh -s -- -y \
        --profile minimal \
        --default-toolchain $TOOLCHAIN \
        --target $TARGET

# Expect our source code to live in /home/rust/src.  We'll run the build as
# user `rust`, which will be uid 1000, gid 1000 outside the container.
RUN mkdir -p /home/rust/src
WORKDIR /home/rust/src
# Add our source code.
ADD --chown=rust:rust build-image/ ./

# --out-dir is not yet stable
ENV RUSTC_BOOTSTRAP=1
# Build our application.
RUN ["cargo", "build", "--target", "x86_64-unknown-linux-musl", "--out-dir=bin", "--release", "-Zunstable-options"]

FROM scratch as empty

FROM scratch

COPY --from=skopeo /go/src/out/skopeo /kaniko-bootstrap/skopeo
COPY --from=kaniko /go/src/out/executor /kaniko-bootstrap/executor
COPY --from=build-image \
    /home/rust/src/bin/build-image \
    /kaniko-bootstrap/build-image

ADD https://mkcert.org/generate/ /kaniko-bootstrap/ssl/certs/ca-certificats.crt
ENV SSL_CERT_DIR=/kaniko/ssl/certs

ADD policy.json /kaniko-bootstrap/containers/policy.json

ENV HOME /root
ENV USER /root
WORKDIR /workspace

ENV PATH /usr/local/bin:/kaniko

VOLUME /workspace
ENTRYPOINT ["/kaniko-bootstrap/build-image"]
