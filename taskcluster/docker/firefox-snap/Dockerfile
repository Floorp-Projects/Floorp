## The below code is from snapcraft/docker/stable.Dockerfile
## The modifications done are part of the documentation for enabling core18 snaps.
## https://snapcraft.io/docs/t/creating-docker-images-for-snapcraft/11739

# /!\ The base image must follow the version of the `coreXX` package
# https://forum.snapcraft.io/t/issues-dockerizing-a-snapcraft-build-process/30294/5
FROM ubuntu:focal

ENV LANG='en_US.UTF-8' \
    LANGUAGE='en_US:en' \
    LC_ALL='en_US.UTF-8' \
    PATH="/snap/bin:$PATH" \
    SNAP='/snap/snapcraft/current' \
    SNAP_NAME='snapcraft' \
    SNAP_ARCH='amd64' \
    TERM='dumb' \
    TZ='Etc/UTC'

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Grab dependencies. snapd is now required per https://github.com/snapcore/snapcraft/pull/3210
RUN apt-get update && \
    apt-get dist-upgrade --yes && \
    apt-get install --yes \
      curl \
      jq \
      squashfs-tools \
      locales \
      bzip2 \
      curl \
      gcc \
      git \
      python3 \
      locales \
      snapd \
      sudo \
      && \
locale-gen "$LANG"

COPY download_and_install_snap.sh .

RUN bash download_and_install_snap.sh 'core20'
RUN bash download_and_install_snap.sh 'gnome-3-38-2004'
RUN bash download_and_install_snap.sh 'gnome-3-38-2004-sdk'
RUN bash download_and_install_snap.sh 'snapcraft'

# Fix Python3 installation: Make sure we use the interpreter from
# the snapcraft snap:
RUN unlink /snap/snapcraft/current/usr/bin/python3 && \
    ln -s /snap/snapcraft/current/usr/bin/python3.* /snap/snapcraft/current/usr/bin/python3 && \
    echo /snap/snapcraft/current/lib/python3.*/site-packages >> /snap/snapcraft/current/usr/lib/python3/dist-packages/site-packages.pth

# Create a snapcraft runner (TODO: move version detection to the core of snapcraft).
RUN mkdir -p /snap/bin
RUN echo "#!/bin/sh" > /snap/bin/snapcraft
RUN snap_version="$(awk '/^version:/{print $2}' /snap/snapcraft/current/meta/snap.yaml)" && echo "export SNAP_VERSION=\"$snap_version\"" >> /snap/bin/snapcraft
RUN echo 'exec "$SNAP/usr/bin/python3" "$SNAP/bin/snapcraft" "$@"' >> /snap/bin/snapcraft
RUN chmod +x /snap/bin/snapcraft

RUN mkdir /scripts/
WORKDIR /scripts/
# Copy everything in the docker/firefox-snap folder but the Dockerfile
#
# XXX The following pattern is neither a regex nor a glob one. It's
# documented at https://golang.org/pkg/path/filepath/#Match. There's no
# way of explicitly filtering out "Dockerfile". If one day, someone needs
# to add a file starting with "D", then we must revisit the pattern below.
COPY [^D]* /scripts/

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
