## The below code is from snapcraft/docker/stable.Dockerfile
## The modifications done are part of the documentation for enabling core18 snaps.
## https://snapcraft.io/docs/t/creating-docker-images-for-snapcraft/11739

FROM ubuntu:bionic

# Grab dependencies
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
      sudo && \
    apt-get clean && \
locale-gen en_US.UTF-8

ENV LANG="en_US.UTF-8"
ENV LANGUAGE="en_US:en"
ENV LC_ALL="en_US.UTF-8"
ENV PATH="/snap/bin:$PATH"
ENV SNAP="/snap/snapcraft/current"
ENV SNAP_NAME="snapcraft"
ENV SNAP_ARCH="amd64"

# Grab the core snap (for backwards compatibility) from the stable channel and
# unpack it in the proper place.
RUN curl -L $(curl -H 'X-Ubuntu-Series: 16' 'https://api.snapcraft.io/api/v1/snaps/details/core' | jq '.download_url' -r) --output core.snap && \
    mkdir -p /snap/core && \
    unsquashfs -d /snap/core/current core.snap && \
    rm core.snap

# Grab the core18 snap (which snapcraft uses as a base) from the stable channel
# and unpack it in the proper place.
RUN curl -L $(curl -H 'X-Ubuntu-Series: 16' 'https://api.snapcraft.io/api/v1/snaps/details/core18' | jq '.download_url' -r) --output core18.snap
RUN mkdir -p /snap/core18
RUN unsquashfs -d /snap/core18/current core18.snap

# Grab the gnome-3-34-1804 snap from the stable channel
# and unpack it in the proper place.
RUN curl -L $(curl -H 'X-Ubuntu-Series: 16' 'https://api.snapcraft.io/api/v1/snaps/details/gnome-3-34-1804?channel=stable' | jq '.download_url' -r) --output gnome-3-34-1804.snap
RUN mkdir -p /snap/gnome-3-34-1804
RUN unsquashfs -d /snap/gnome-3-34-1804/current gnome-3-34-1804.snap

# Grab the gnome-3-34-1804-sdk snap from the stable channel
# and unpack it in the proper place.
RUN curl -L $(curl -H 'X-Ubuntu-Series: 16' 'https://api.snapcraft.io/api/v1/snaps/details/gnome-3-34-1804-sdk?channel=stable' | jq '.download_url' -r) --output gnome-3-34-1804-sdk.snap
RUN mkdir -p /snap/gnome-3-34-1804-sdk
RUN unsquashfs -d /snap/gnome-3-34-1804-sdk/current gnome-3-34-1804-sdk.snap

# Grab the snapcraft snap from the stable channel and unpack it in the proper
# place.
RUN curl -L $(curl -H 'X-Ubuntu-Series: 16' 'https://api.snapcraft.io/api/v1/snaps/details/snapcraft?channel=stable' | jq '.download_url' -r) --output snapcraft.snap && \
    mkdir -p /snap/snapcraft && \
    unsquashfs -d /snap/snapcraft/current snapcraft.snap && \
    rm snapcraft.snap

# Create a snapcraft runner (TODO: move version detection to the core of
# snapcraft).
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
