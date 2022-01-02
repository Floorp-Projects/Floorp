#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

QEMU_RELEASE="4.1.0"
QEMU_URL="https://download.qemu.org/qemu-${QEMU_RELEASE}.tar.xz"
QEMU_ARCHS=(
  aarch64
  arm
  i386
  # TODO: Consider adding these:
  # aarch64_be
  # mips64el
  # mips64
  # mips
  # ppc64
  # ppc
)

# Ubuntu packages not installed that are needed to build qemu.
QEMU_BUILD_DEPS=(
  libglib2.0-dev
  libpixman-1-dev
  flex
  bison
)

set -eu -x

# Temporary files cleanup hooks.
CLEANUP_FILES=()
cleanup() {
  if [[ ${#CLEANUP_FILES[@]} -ne 0 ]]; then
    rm -fr "${CLEANUP_FILES[@]}"
  fi
}
trap "{ set +x; } 2>/dev/null; cleanup" INT TERM EXIT

main() {
  local workdir=$(mktemp -d --suffix=qemu)
  CLEANUP_FILES+=("${workdir}")

  apt install -y "${QEMU_BUILD_DEPS[@]}"

  local qemutar="${workdir}/qemu.tar.gz"
  curl --output "${qemutar}" "${QEMU_URL}"
  tar -Jxf "${qemutar}" -C "${workdir}"
  local srcdir="${workdir}/qemu-${QEMU_RELEASE}"

  local builddir="${workdir}/build"
  local prefixdir="${workdir}/prefix"
  mkdir -p "${builddir}"

  # List of targets to build.
  local targets=""
  local make_targets=()
  local target
  for target in "${QEMU_ARCHS[@]}"; do
    targets="${targets} ${target}-linux-user"
    # Build just the linux-user targets.
    make_targets+=("${target}-linux-user/all")
  done

  cd "${builddir}"
  "${srcdir}/configure" \
    --prefix="${prefixdir}" \
    --static --disable-system --enable-linux-user \
    --target-list="${targets}"

  make -j $(nproc --all || echo 1) "${make_targets[@]}"

  # Manually install these into the non-standard location. This script runs as
  # root anyway.
  for target in "${QEMU_ARCHS[@]}"; do
    cp "${target}-linux-user/qemu-${target}" "/usr/bin/qemu-${target}-static"
  done

  apt autoremove -y --purge "${QEMU_BUILD_DEPS[@]}"
}

main "$@"
