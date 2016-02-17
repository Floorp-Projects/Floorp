FROM          quay.io/rillian/rust-buildbot
MAINTAINER    Ralph Giles <giles@mozilla.com>

# Reset user/workdir from parent image so we can install software.
WORKDIR /
USER root

# Update base.
RUN yum upgrade -y
RUN yum clean all

# Install tooltool directly from github.
RUN mkdir /builds
ADD https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py /build/tooltool.py
RUN chmod +rx /build/tooltool.py

# Add build scripts.
ADD             fetch_rust.sh build_rust.sh upload_rust.sh /build/
RUN             chmod +x /build/*

# Create user for doing the build.
ENV USER worker
ENV HOME /home/${USER}

RUN useradd -d ${HOME} -m ${USER}

# Set up the user's tree
WORKDIR ${HOME}

# Invoke our build scripts by default, but allow other commands.
USER ${USER}
ENTRYPOINT /build/fetch_rust.sh && /build/build_rust.sh && /build/upload_rust.sh
