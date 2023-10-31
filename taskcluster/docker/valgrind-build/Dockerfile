FROM $DOCKER_IMAGE_PARENT
MAINTAINER Mike Hommey <mhommey@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ARG TASKCLUSTER_ROOT_URL
ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $TASKCLUSTER_ROOT_URL $DOCKER_IMAGE_PACKAGES

# We could try to be smart and install all the -dbg packages corresponding to
# the installed packages, but, not all of them are actually for libraries used
# by Firefox, leading to a larger docker image. Moreover, some of the -dbg
# packages for unnecessary libraries pull other packages through dependencies,
# that make for even larger docker images.
RUN apt-get update && apt-get install \
    dbus-x11 \
    libgtk-3-bin \
    libdbus-1-3-dbgsym \
    libatk-bridge2.0-0-dbgsym \
    libatk1.0-0-dbgsym \
    libcairo2-dbgsym \
    libdrm2-dbgsym \
    libffi8-dbgsym \
    libfontconfig1-dbgsym \
    libglx-mesa0-dbgsym \
    libglapi-mesa-dbgsym \
    libglib2.0-0-dbgsym \
    libgdk-pixbuf-2.0-0-dbgsym \
    libgtk-3-0-dbgsym \
    libice6-dbgsym \
    libicu72-dbgsym \
    libpango-1.0-0-dbgsym \
    libpcre3-dbg \
    libpixman-1-0-dbgsym \
    libsm6-dbgsym \
    libvorbis0a-dbgsym \
    libwayland-client0-dbgsym \
    libwayland-cursor0-dbgsym \
    libx11-6-dbgsym \
    libx11-xcb1-dbgsym \
    libxau6-dbgsym \
    libxcb-glx0-dbgsym \
    libxcb-render0-dbgsym \
    libxcb-shm0-dbgsym \
    libxcb1-dbgsym \
    libxcomposite1-dbgsym \
    libxcursor1-dbgsym \
    libxdamage1-dbgsym \
    libxdmcp6-dbg \
    libxext6-dbgsym \
    libxfixes3-dbgsym \
    libxi6-dbgsym \
    libxinerama1-dbgsym \
    libxrandr2-dbgsym \
    libxrender1-dbgsym \
    libxt6-dbgsym \
    libxxf86vm1-dbg \
    valgrind-dbgsym \
    xvfb \
    zlib1g-dbgsym

# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh
