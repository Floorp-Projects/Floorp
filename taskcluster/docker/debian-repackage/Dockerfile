FROM $DOCKER_IMAGE_PARENT
MAINTAINER Mozilla Releng <release@mozilla.com>

VOLUME /builds/worker/workspace

# At the moment the Firefox build sysroot for i386 and amd64 use jessie and arm64 uses buster.
# To generate Firefox Debian packages we bootstrap jessie and buster sysroots.
# We use these sysroots to generate shared library dependencies for
# i386 (jessie), amd64 (jessie), and arm64 (buster) packages.
# To keep the build and repackage enviroments consistent the Debian distributions used by the sysroots
# created here should match the distributions used by the sysroots created in taskcluster/scripts/misc/build-sysroot.sh
ARG TASKCLUSTER_ROOT_URL
ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $TASKCLUSTER_ROOT_URL $DOCKER_IMAGE_PACKAGES && \
    dpkg --add-architecture arm64 && \
    apt-get update && \
    apt-get install debootstrap && set -xe && \
    for arch in i386 amd64; do \
        debootstrap \
        --arch=$arch \
        --variant=buildd \
        --include=debhelper,libasound2,libgtk-3-0,libx11-xcb1,libxtst6 \
        --keyring=/usr/share/keyrings/debian-archive-removed-keys.gpg \
        --verbose \
        jessie \
        /srv/jessie-$arch \
        https://archive.debian.org/debian || (echo "Debootstrap failed, outputting log:" && cat /srv/jessie-$arch/debootstrap/debootstrap.log && exit 1); \
    done && \
    # bootstrap a buster/amd64 root to generate an arm64 .deb package.
    debootstrap \
    --arch=amd64 \
    --variant=buildd \
    --include=debhelper,binutils-multiarch,binutils-aarch64-linux-gnu \
    --keyring=/usr/share/keyrings/debian-archive-buster-stable.gpg \
    --verbose \
    buster \
    /srv/buster-amd64 \
    http://deb.debian.org/debian || (echo "Debootstrap failed, outputting log:" && cat /srv/buster-amd64/debootstrap/debootstrap.log && exit 1) && \
    chroot /srv/buster-amd64 /bin/bash -c "\
        dpkg --add-architecture arm64 && \
        apt-get update && \
        apt-get install -y libasound2:arm64 libgtk-3-0:arm64 libx11-xcb1:arm64 libxtst6:arm64" && \
    /usr/local/sbin/clean_packages.sh $DOCKER_IMAGE_PACKAGES && \
    # Alter the symbols file for libgdk-pixbuf to handle the transition to libgdk-pixbuf-2.0-0
    # This is only necessary until we upgrade to something newer than buster as sysroot.
    find /srv/ -name libgdk-pixbuf2.0-0*symbols | xargs sed -i "/libgdk-pixbuf2.0-0/s/libgdk-pixbuf2.0-0/libgdk-pixbuf2.0-0 #MINVER# | libgdk-pixbuf-2.0-0/"
