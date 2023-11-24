FROM $DOCKER_IMAGE_PARENT
MAINTAINER Mozilla Releng <release@mozilla.com>

VOLUME /builds/worker/workspace

# At the moment the Firefox build baseline is jessie.
# Things in the build/CI system that run in the task's container
# (mach, run-task, etc.) do not support jessie.
# There's also no jessie `base` images or packages generated in the CI.
# To generate Firefox Debian packages compatible with the baseline
# we bootstrap basic i386 and amd64 jessie systems on bullseye.
# We use these to generate shared library dependencies in jessie using chroot.
# python/mozbuild/mozbuild/repackaging/deb.py checks for jessie systems
# bootstrapped under /srv/jessie-i386 and /srv/jessie-amd64
# If they aren't there, deb.py runs the repackage in the working directory without using chroot.
# To keep the build and repackage enviroments consistent the Debian baseline used
# here (jessie) should be synchronized with the baseline used in
# taskcluster/scripts/misc/build-sysroot.sh
ARG TASKCLUSTER_ROOT_URL
ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $TASKCLUSTER_ROOT_URL $DOCKER_IMAGE_PACKAGES && \
    apt-get update && \
    apt-get install debootstrap python3-distutils && set -xe && \
    for arch in i386 amd64; do \
        debootstrap \
        --arch=$arch \
        --variant=buildd \
        --include=debhelper,libasound2,libgtk-3-0,libx11-xcb1,libxtst6 \
        --keyring=/usr/share/keyrings/debian-archive-removed-keys.gpg \
        --verbose \
        jessie \
        /srv/jessie-$arch \
        https://archive.debian.org/debian; \
    done && \
    /usr/local/sbin/clean_packages.sh $DOCKER_IMAGE_PACKAGES
