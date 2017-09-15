#!/usr/bin/env bash

SYSROOT=${HOME}/x-tools/armv6-rpi-linux-gnueabi/armv6-rpi-linux-gnueabi/sysroot

export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig:${SYSROOT}/usr/share/pkgconfig/
export PKG_CONFIG_SYSROOT_DIR=${SYSROOT}

export PKG_CONFIG_ALLOW_CROSS=1
