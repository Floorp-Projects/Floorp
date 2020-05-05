# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Mike Hommey <mhommey@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

# We could try to be smart and install all the -dbg packages corresponding to
# the installed packages, but, not all of them are actually for libraries used
# by Firefox, leading to a larger docker image. Moreover, some of the -dbg
# packages for unnecessary libraries pull other packages through dependencies,
# that make for even larger docker images.
RUN apt-get install \
    dbus-1-dbg \
    libatk-bridge2.0-0-dbg \
    libatk1.0-dbg \
    libcairo2-dbg \
    libdbus-glib-1-2-dbg \
    libdrm2-dbg \
    libffi6-dbg \
    libfontconfig1-dbg \
    libgl1-mesa-glx-dbg \
    libglapi-mesa-dbg \
    libglib2.0-0-dbg \
    libgdk-pixbuf2.0-0-dbg \
    libgtk-3-0-dbg \
    libice6-dbg \
    libicu52-dbg \
    libpango1.0-0-dbg \
    libpcre3-dbg \
    libpixman-1-0-dbg \
    libsm6-dbg \
    libvorbis-dbg \
    libwayland-client0-dbg \
    libwayland-cursor0-dbg \
    libx11-6-dbg \
    libx11-xcb1-dbg \
    libxau6-dbg \
    libxcb-glx0-dbg \
    libxcb-render0-dbg \
    libxcb-shm0-dbg \
    libxcb1-dbg \
    libxcomposite1-dbg \
    libxcursor1-dbg \
    libxdamage1-dbg \
    libxdmcp6-dbg \
    libxext6-dbg \
    libxfixes3-dbg \
    libxi6-dbg \
    libxinerama1-dbg \
    libxrandr2-dbg \
    libxrender1-dbg \
    libxt6-dbg \
    libxxf86vm1-dbg \
    valgrind-dbg \
    zlib1g-dbg
