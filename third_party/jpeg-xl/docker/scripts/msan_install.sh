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

set -eu

MYDIR=$(dirname $(realpath "$0"))

# Convenience flag to pass both CMAKE_C_FLAGS and CMAKE_CXX_FLAGS
CMAKE_FLAGS=${CMAKE_FLAGS:-}
CMAKE_C_FLAGS=${CMAKE_C_FLAGS:-${CMAKE_FLAGS}}
CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS:-${CMAKE_FLAGS}}
CMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS:-}

CLANG_VERSION="${CLANG_VERSION:-}"
# Detect the clang version suffix and store it in CLANG_VERSION. For example,
# "6.0" for clang 6 or "7" for clang 7.
detect_clang_version() {
  if [[ -n "${CLANG_VERSION}" ]]; then
    return 0
  fi
  local clang_version=$("${CC:-clang}" --version | head -n1)
  local llvm_tag
  case "${clang_version}" in
    "clang version 6."*)
      CLANG_VERSION="6.0"
      ;;
    "clang version 7."*)
      CLANG_VERSION="7"
      ;;
    "clang version 8."*)
      CLANG_VERSION="8"
      ;;
    "clang version 9."*)
      CLANG_VERSION="9"
      ;;
    *)
      echo "Unknown clang version: ${clang_version}" >&2
      return 1
  esac
}

# Temporary files cleanup hooks.
CLEANUP_FILES=()
cleanup() {
  if [[ ${#CLEANUP_FILES[@]} -ne 0 ]]; then
    rm -fr "${CLEANUP_FILES[@]}"
  fi
}
trap "{ set +x; } 2>/dev/null; cleanup" INT TERM EXIT

# Install libc++ libraries compiled with msan in the msan_prefix for the current
# compiler version.
cmd_msan_install() {
  local tmpdir=$(mktemp -d)
  CLEANUP_FILES+=("${tmpdir}")
  # Detect the llvm to install:
  export CC="${CC:-clang}"
  export CXX="${CXX:-clang++}"
  detect_clang_version
  local llvm_tag
  case "${CLANG_VERSION}" in
    "6.0")
      llvm_tag="llvmorg-6.0.1"
      ;;
    "7")
      llvm_tag="llvmorg-7.0.1"
      ;;
    "8")
      llvm_tag="llvmorg-8.0.0"
      ;;
    *)
      echo "Unknown clang version: ${clang_version}" >&2
      return 1
  esac
  local llvm_targz="${tmpdir}/${llvm_tag}.tar.gz"
  curl -L --show-error -o "${llvm_targz}" \
    "https://github.com/llvm/llvm-project/archive/${llvm_tag}.tar.gz"
  tar -C "${tmpdir}" -zxf "${llvm_targz}"
  local llvm_root="${tmpdir}/llvm-project-${llvm_tag}"

  local msan_prefix="${HOME}/.msan/${CLANG_VERSION}"
  rm -rf "${msan_prefix}"

  declare -A CMAKE_EXTRAS
  CMAKE_EXTRAS[libcxx]="\
    -DLIBCXX_CXX_ABI=libstdc++ \
    -DLIBCXX_INSTALL_EXPERIMENTAL_LIBRARY=ON"

  for project in libcxx; do
    local proj_build="${tmpdir}/build-${project}"
    local proj_dir="${llvm_root}/${project}"
    mkdir -p "${proj_build}"
    cmake -B"${proj_build}" -H"${proj_dir}" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_USE_SANITIZER=Memory \
      -DLLVM_PATH="${llvm_root}/llvm" \
      -DLLVM_CONFIG_PATH="$(which llvm-config llvm-config-7 llvm-config-6.0 | \
                            head -n1)" \
      -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS}" \
      -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS}" \
      -DCMAKE_EXE_LINKER_FLAGS="${CMAKE_EXE_LINKER_FLAGS}" \
      -DCMAKE_INSTALL_PREFIX="${msan_prefix}" \
      ${CMAKE_EXTRAS[${project}]}
    cmake --build "${proj_build}"
    ninja -C "${proj_build}" install
  done
}

main() {
  set -x
  for version in 6.0 7 8; do
    if ! which "clang-${version}" >/dev/null; then
      echo "Skipping msan install for clang version ${version}"
      continue
    fi
    (
     trap "{ set +x; } 2>/dev/null; cleanup" INT TERM EXIT
     export CLANG_VERSION=${version}
     export CC=clang-${version}
     export CXX=clang++-${version}
     cmd_msan_install
    ) &
  done
  wait
}

main "$@"
