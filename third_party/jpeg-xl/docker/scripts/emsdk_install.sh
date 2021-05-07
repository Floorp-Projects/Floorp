#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
