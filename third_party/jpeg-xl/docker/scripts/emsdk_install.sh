#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

EMSDK_URL="https://github.com/emscripten-core/emsdk/archive/master.tar.gz"
EMSDK_DIR="/opt/emsdk"

EMSDK_RELEASE="2.0.4"

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
  local workdir=$(mktemp -d --suffix=emsdk)
  CLEANUP_FILES+=("${workdir}")

  local emsdktar="${workdir}/emsdk.tar.gz"
  curl --output "${emsdktar}" "${EMSDK_URL}" --location
  mkdir -p "${EMSDK_DIR}"
  tar -zxf "${emsdktar}" -C "${EMSDK_DIR}" --strip-components=1

  cd "${EMSDK_DIR}"
  ./emsdk install --shallow "${EMSDK_RELEASE}"
  ./emsdk activate --embedded "${EMSDK_RELEASE}"
}

main "$@"
