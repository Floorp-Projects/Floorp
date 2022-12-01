#!/bin/bash

MAJOR_VERSION=$(grep -E "dav1d_soname_version\s+=" meson.build | grep -o -E "'([0-9]+)" | sed "s/'//")
MINOR_VERSION=$(grep -E "dav1d_soname_version\s+=" meson.build | grep -o -E "\.([0-9]+)\." | sed "s/\.//g")
PATCH_VERSION=$(grep -E "dav1d_soname_version\s+=" meson.build | grep -o -E "([0-9]+)'" | sed "s/'//")

sed -i.bak "s/DAV1D_API_VERSION_MAJOR \([0-9]\+\)/DAV1D_API_VERSION_MAJOR $MAJOR_VERSION/" "$1"
sed -i.bak "s/DAV1D_API_VERSION_MINOR \([0-9]\+\)/DAV1D_API_VERSION_MINOR $MINOR_VERSION/" "$1"
sed -i.bak "s/DAV1D_API_VERSION_PATCH \([0-9]\+\)/DAV1D_API_VERSION_PATCH $PATCH_VERSION/" "$1"
rm -f "$1.bak"
