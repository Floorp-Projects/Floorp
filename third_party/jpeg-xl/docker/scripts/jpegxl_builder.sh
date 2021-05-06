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

# Main entry point for all the Dockerfile for jpegxl-builder. This centralized
# file helps sharing code and configuration between Dockerfiles.

set -eux

MYDIR=$(dirname $(realpath "$0"))

# libjpeg-turbo.
JPEG_TURBO_RELEASE="2.0.4"
JPEG_TURBO_URL="https://github.com/libjpeg-turbo/libjpeg-turbo/archive/${JPEG_TURBO_RELEASE}.tar.gz"
JPEG_TURBO_SHA256="7777c3c19762940cff42b3ba4d7cd5c52d1671b39a79532050c85efb99079064"

# zlib (dependency of libpng)
ZLIB_RELEASE="1.2.11"
ZLIB_URL="https://www.zlib.net/zlib-${ZLIB_RELEASE}.tar.gz"
ZLIB_SHA256="c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1"
# The name in the .pc and the .dll generated don't match in zlib for Windows
# because they use different .dll names in Windows. We avoid that by defining
# UNIX=1. We also install all the .dll files to ${prefix}/lib instead of the
# default ${prefix}/bin.
ZLIB_FLAGS='-DUNIX=1 -DINSTALL_PKGCONFIG_DIR=/${CMAKE_INSTALL_PREFIX}/lib/pkgconfig -DINSTALL_BIN_DIR=/${CMAKE_INSTALL_PREFIX}/lib'

# libpng
LIBPNG_RELEASE="1.6.37"
LIBPNG_URL="https://github.com/glennrp/libpng/archive/v${LIBPNG_RELEASE}.tar.gz"
LIBPNG_SHA256="ca74a0dace179a8422187671aee97dd3892b53e168627145271cad5b5ac81307"

# giflib
GIFLIB_RELEASE="5.2.1"
GIFLIB_URL="https://netcologne.dl.sourceforge.net/project/giflib/giflib-${GIFLIB_RELEASE}.tar.gz"
GIFLIB_SHA256="31da5562f44c5f15d63340a09a4fd62b48c45620cd302f77a6d9acf0077879bd"

# A patch needed to compile GIFLIB in mingw.
GIFLIB_PATCH_URL="https://github.com/msys2/MINGW-packages/raw/3afde38fcee7b3ba2cafd97d76cca8f06934504f/mingw-w64-giflib/001-mingw-build.patch"
GIFLIB_PATCH_SHA256="2b2262ddea87fc07be82e10aeb39eb699239f883c899aa18a16e4d4e40af8ec8"

# webp
WEBP_RELEASE="1.0.2"
WEBP_URL="https://codeload.github.com/webmproject/libwebp/tar.gz/v${WEBP_RELEASE}"
WEBP_SHA256="347cf85ddc3497832b5fa9eee62164a37b249c83adae0ba583093e039bf4881f"

# Google benchmark
BENCHMARK_RELEASE="1.5.2"
BENCHMARK_URL="https://github.com/google/benchmark/archive/v${BENCHMARK_RELEASE}.tar.gz"
BENCHMARK_SHA256="dccbdab796baa1043f04982147e67bb6e118fe610da2c65f88912d73987e700c"
BENCHMARK_FLAGS="-DGOOGLETEST_PATH=${MYDIR}/../../third_party/googletest"
# attribute(format(__MINGW_PRINTF_FORMAT, ...)) doesn't work in our
# environment, so we disable the warning.
BENCHMARK_FLAGS="-DCMAKE_BUILD_TYPE=Release -DBENCHMARK_ENABLE_TESTING=OFF \
  -DCMAKE_CXX_FLAGS=-Wno-ignored-attributes \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON"

# V8
V8_VERSION="8.7.230"

# Temporary files cleanup hooks.
CLEANUP_FILES=()
cleanup() {
  if [[ ${#CLEANUP_FILES[@]} -ne 0 ]]; then
    rm -fr "${CLEANUP_FILES[@]}"
  fi
}
trap "{ set +x; } 2>/dev/null; cleanup" INT TERM EXIT

# List of Ubuntu arch names supported by the builder (such as "i386").
LIST_ARCHS=(
  amd64
  i386
  arm64
  armhf
)

# List of target triplets supported by the builder.
LIST_TARGETS=(
  x86_64-linux-gnu
  i686-linux-gnu
  arm-linux-gnueabihf
  aarch64-linux-gnu
)
LIST_MINGW_TARGETS=(
  i686-w64-mingw32
  x86_64-w64-mingw32
)
LIST_WASM_TARGETS=(
  wasm32
)

# Setup the apt repositories and supported architectures.
setup_apt() {
  apt-get update -y
  apt-get install -y curl gnupg ca-certificates

  apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 1E9377A2BA9EF27F

  # node sources.
  cat >/etc/apt/sources.list.d/nodesource.list <<EOF
  deb https://deb.nodesource.com/node_14.x bionic main
  deb-src https://deb.nodesource.com/node_14.x bionic main
EOF
  curl -s https://deb.nodesource.com/gpgkey/nodesource.gpg.key | apt-key add -

  local port_list=()
  local main_list=()
  local ubarch
  for ubarch in "${LIST_ARCHS[@]}"; do
    if [[ "${ubarch}" != "amd64" && "${ubarch}" != "i386" ]]; then
      # other archs are not part of the main mirrors, but available in
      # ports.ubuntu.com.
      port_list+=("${ubarch}")
    else
      main_list+=("${ubarch}")
    fi
    # Add the arch to the system.
    if [[ "${ubarch}" != "amd64" ]]; then
      dpkg --add-architecture "${ubarch}"
    fi
  done

  # Update the sources.list with the split of supported architectures.
  local bkplist="/etc/apt/sources.list.bkp"
  [[ -e "${bkplist}" ]] || \
    mv /etc/apt/sources.list "${bkplist}"

  local newlist="/etc/apt/sources.list.tmp"
  rm -f "${newlist}"
  port_list=$(echo "${port_list[@]}" | tr ' ' ,)
  if [[ -n "${port_list}" ]]; then
    local port_url="http://ports.ubuntu.com/ubuntu-ports/"
    grep -v -E '^#' "${bkplist}" |
      sed -E "s;^deb (http[^ ]+) (.*)\$;deb [arch=${port_list}] ${port_url} \\2;" \
      >>"${newlist}"
  fi

  main_list=$(echo "${main_list[@]}" | tr ' ' ,)
  grep -v -E '^#' "${bkplist}" |
    sed -E "s;^deb (http[^ ]+) (.*)\$;deb [arch=${main_list}] \\1 \\2\ndeb-src [arch=${main_list}] \\1 \\2;" \
    >>"${newlist}"
  mv "${newlist}" /etc/apt/sources.list
}

install_pkgs() {
  packages=(
    # Native compilers (minimum for SIMD is clang-7)
    clang-7 clang-format-7 clang-tidy-7

    # TODO: Consider adding clang-8 to every builder:
    #   clang-8 clang-format-8 clang-tidy-8

    # For cross-compiling to Windows with mingw.
    mingw-w64
    wine64
    wine-binfmt

    # Native tools.
    bsdmainutils
    cmake
    extra-cmake-modules
    git
    llvm
    nasm
    ninja-build
    parallel
    pkg-config

    # These are used by the ./ci.sh lint in the native builder.
    clang-format-7
    clang-format-8

    # For coverage builds
    gcovr

    # For compiling giflib documentation.
    xmlto

    # Common libraries.
    libstdc++-8-dev

    # We don't use tcmalloc on archs other than amd64. This installs
    # libgoogle-perftools4:amd64.
    google-perftools

    # NodeJS for running WASM tests
    nodejs

    # To generate API documentation.
    doxygen

    # Freezes version that builds (passes tests). Newer version
    # (2.30-21ubuntu1~18.04.4) claims to fix "On Intel Skylake
    # (-march=native) generated avx512 instruction can be wrong",
    # but newly added tests does not pass. Perhaps the problem is
    # that mingw package is not updated.
    binutils-source=2.30-15ubuntu1
  )

  # Install packages that are arch-dependent.
  local ubarch
  for ubarch in "${LIST_ARCHS[@]}"; do
    packages+=(
      # Library dependencies. These normally depend on the target architecture
      # we are compiling for and can't usually be installed for multiple
      # architectures at the same time.
      libgif7:"${ubarch}"
      libjpeg-dev:"${ubarch}"
      libpng-dev:"${ubarch}"
      libqt5x11extras5-dev:"${ubarch}"

      libstdc++-8-dev:"${ubarch}"
      qtbase5-dev:"${ubarch}"

      # For OpenEXR:
      libilmbase12:"${ubarch}"
      libopenexr22:"${ubarch}"

      # TCMalloc dependency
      libunwind-dev:"${ubarch}"

      # Cross-compiling tools per arch.
      libc6-dev-"${ubarch}"-cross
      libstdc++-8-dev-"${ubarch}"-cross
    )
  done

  local target
  for target in "${LIST_TARGETS[@]}"; do
    # Per target cross-compiling tools.
    if [[ "${target}" != "x86_64-linux-gnu" ]]; then
      packages+=(
        binutils-"${target}"
        gcc-"${target}"
      )
    fi
  done

  # Install all the manual packages via "apt install" for the main arch. These
  # will be installed for other archs via manual download and unpack.
  apt install -y "${packages[@]}" "${UNPACK_PKGS[@]}"
}

# binutils <2.32 need a patch.
install_binutils() {
  local workdir=$(mktemp -d --suffix=_install)
  CLEANUP_FILES+=("${workdir}")
  pushd "${workdir}"
  apt source binutils-mingw-w64
  apt -y build-dep binutils-mingw-w64
  cd binutils-mingw-w64-8ubuntu1
  cp "${MYDIR}/binutils_align_fix.patch" debian/patches
  echo binutils_align_fix.patch >> debian/patches/series
  dpkg-buildpackage -b
  cd ..
  dpkg -i *deb
  popd
}

# Install a library from the source code for multiple targets.
# Usage: install_from_source <tar_url> <sha256> <target> [<target...>]
install_from_source() {
  local package="$1"
  shift

  local url
  eval "url=\${${package}_URL}"
  local sha256
  eval "sha256=\${${package}_SHA256}"
  # Optional package flags
  local pkgflags
  eval "pkgflags=\${${package}_FLAGS:-}"

  local workdir=$(mktemp -d --suffix=_install)
  CLEANUP_FILES+=("${workdir}")

  local tarfile="${workdir}"/$(basename "${url}")
  curl -L --output "${tarfile}" "${url}"
  if ! echo "${sha256} ${tarfile}" | sha256sum -c --status -; then
    echo "SHA256 mismatch for ${url}: expected ${sha256} but found:"
    sha256sum "${tarfile}"
    exit 1
  fi

  local target
  for target in "$@"; do
    echo "Installing ${package} for target ${target} from ${url}"

    local srcdir="${workdir}/source-${target}"
    mkdir -p "${srcdir}"
    tar -zxf "${tarfile}" -C "${srcdir}" --strip-components=1

    local prefix="/usr"
    if [[ "${target}" != "x86_64-linux-gnu" ]]; then
      prefix="/usr/${target}"
    fi

    # Apply patches to buildfiles.
    if [[ "${package}" == "GIFLIB" && "${target}" == *mingw32 ]]; then
      # GIFLIB Makefile has several problems so we need to fix them here. We are
      # using a patch from MSYS2 that already fixes the compilation for mingw.
      local make_patch="${srcdir}/libgif.patch"
      curl -L "${GIFLIB_PATCH_URL}" -o "${make_patch}"
      echo "${GIFLIB_PATCH_SHA256} ${make_patch}" | sha256sum -c --status -
      patch "${srcdir}/Makefile" < "${make_patch}"
    elif [[ "${package}" == "LIBPNG" && "${target}" == wasm* ]]; then
      # Cut the dependency to libm; there is pull request to fix it, so this
      # might not be needed in the future.
      sed -i 's/APPLE/EMSCRIPTEN/g' "${srcdir}/CMakeLists.txt"
    fi

    local cmake_args=()
    local export_args=("CC=clang-7" "CXX=clang++-7")
    local cmake="cmake"
    local make="make"
    local system_name="Linux"
    if [[ "${target}" == *mingw32 ]]; then
      system_name="Windows"
      # When compiling with clang, CMake doesn't detect that we are using mingw.
      cmake_args+=(
        -DMINGW=1
        # Googletest needs this when cross-compiling to windows
        -DCMAKE_CROSSCOMPILING=1
        -DHAVE_STD_REGEX=0
        -DHAVE_POSIX_REGEX=0
        -DHAVE_GNU_POSIX_REGEX=0
      )
      local windres=$(which ${target}-windres || true)
      if [[ -n "${windres}" ]]; then
        cmake_args+=(-DCMAKE_RC_COMPILER="${windres}")
      fi
    fi
    if [[ "${target}" == wasm* ]]; then
      system_name="WASM"
      cmake="emcmake cmake"
      make="emmake make"
      export_args=()
      cmake_args+=(
        -DCMAKE_FIND_ROOT_PATH="${prefix}"
        -DCMAKE_PREFIX_PATH="${prefix}"
      )
      # Static and shared library link to the same file -> race condition.
      nproc=1
    else
      nproc=`nproc --all`
    fi
    cmake_args+=(-DCMAKE_SYSTEM_NAME="${system_name}")

    if [[ "${target}" != "x86_64-linux-gnu" ]]; then
      # Cross-compiling.
      cmake_args+=(
        -DCMAKE_C_COMPILER_TARGET="${target}"
        -DCMAKE_CXX_COMPILER_TARGET="${target}"
        -DCMAKE_SYSTEM_PROCESSOR="${target%%-*}"
      )
    fi

    if [[ -e "${srcdir}/CMakeLists.txt" ]]; then
      # Most pacakges use cmake for building which is easier to configure for
      # cross-compiling.
      if [[ "${package}" == "JPEG_TURBO" && "${target}" == wasm* ]]; then
        # JT erroneously detects WASM CPU as i386 and tries to use asm.
        # Wasm/Emscripten support for dynamic linking is incomplete; disable
        # to avoid CMake warning.
        cmake_args+=(-DWITH_SIMD=0 -DENABLE_SHARED=OFF)
      fi
      (
        cd "${srcdir}"
        export ${export_args[@]}
        ${cmake} \
          -DCMAKE_INSTALL_PREFIX="${prefix}" \
          "${cmake_args[@]}" ${pkgflags}
        ${make} -j${nproc}
        ${make} install
      )
    elif [[ "${package}" == "GIFLIB" ]]; then
      # GIFLIB doesn't yet have a cmake build system. There is a pull
      # request in giflib for adding CMakeLists.txt so this might not be
      # needed in the future.
      (
        cd "${srcdir}"
        local giflib_make_flags=(
          CFLAGS="-O2 --target=${target} -std=gnu99"
          PREFIX="${prefix}"
        )
        if [[ "${target}" != wasm* ]]; then
          giflib_make_flags+=(CC=clang-7)
        fi
        # giflib make dependencies are not properly set up so parallel building
        # doesn't work for everything.
        ${make} -j${nproc} libgif.a "${giflib_make_flags[@]}"
        ${make} -j${nproc} all "${giflib_make_flags[@]}"
        ${make} install "${giflib_make_flags[@]}"
      )
    else
      echo "Don't know how to install ${package}"
      exit 1
    fi

  done
}

# Packages that are manually unpacked for each architecture.
UNPACK_PKGS=(
  libgif-dev
  libclang-common-7-dev

  # For OpenEXR:
  libilmbase-dev
  libopenexr-dev

  # TCMalloc
  libgoogle-perftools-dev
  libtcmalloc-minimal4
  libgoogle-perftools4
)

# Main script entry point.
main() {
  cd "${MYDIR}"

  # Configure the repositories with the sources for multi-arch cross
  # compilation.
  setup_apt
  apt-get update -y
  apt-get dist-upgrade -y

  install_pkgs
  install_binutils
  apt clean

  # Manually extract packages for the target arch that can't install it directly
  # at the same time as the native ones.
  local ubarch
  for ubarch in "${LIST_ARCHS[@]}"; do
    if [[ "${ubarch}" != "amd64" ]]; then
      local pkg
      for pkg in "${UNPACK_PKGS[@]}"; do
        apt download "${pkg}":"${ubarch}"
        dpkg -x "${pkg}"_*_"${ubarch}".deb /
      done
    fi
  done
  # TODO: Add clang from the llvm repos. This is problematic since we are
  # installing libclang-common-7-dev:"${ubarch}" from the ubuntu ports repos
  # which is not available in the llvm repos so it might have a different
  # version than the ubuntu ones.

  # Remove the win32 libgcc version. The gcc-mingw-w64-x86-64 (and i686)
  # packages install two libgcc versions:
  #   /usr/lib/gcc/x86_64-w64-mingw32/7.3-posix
  #   /usr/lib/gcc/x86_64-w64-mingw32/7.3-win32
  # (exact libgcc version number depends on the package version).
  #
  # Clang will pick the best libgcc, sorting by version, but it doesn't
  # seem to be a way to specify one or the other one, except by passing
  # -nostdlib and setting all the include paths from the command line.
  # To check which one is being used you can run:
  #   clang++-7 --target=x86_64-w64-mingw32 -v -print-libgcc-file-name
  # We need to use the "posix" versions for thread support, so here we
  # just remove the other one.
  local target
  for target in "${LIST_MINGW_TARGETS[@]}"; do
    update-alternatives --set "${target}-gcc" $(which "${target}-gcc-posix")
    local gcc_win32_path=$("${target}-cpp-win32" -print-libgcc-file-name)
    rm -rf $(dirname "${gcc_win32_path}")
  done

  # TODO: Add msan for the target when cross-compiling. This only installs it
  # for amd64.
  ./msan_install.sh

  # Build and install qemu user-linux targets.
  ./qemu_install.sh

  # Install emscripten SDK.
  ./emsdk_install.sh

  # Setup environment for building WASM libraries from sources.
  source /opt/emsdk/emsdk_env.sh

  # Install some dependency libraries manually for the different targets.

  install_from_source JPEG_TURBO "${LIST_MINGW_TARGETS[@]}" "${LIST_WASM_TARGETS[@]}"
  install_from_source ZLIB "${LIST_MINGW_TARGETS[@]}" "${LIST_WASM_TARGETS[@]}"
  install_from_source LIBPNG "${LIST_MINGW_TARGETS[@]}" "${LIST_WASM_TARGETS[@]}"
  install_from_source GIFLIB "${LIST_MINGW_TARGETS[@]}" "${LIST_WASM_TARGETS[@]}"
  # webp in Ubuntu is relatively old so we install it from source for everybody.
  install_from_source WEBP "${LIST_TARGETS[@]}" "${LIST_MINGW_TARGETS[@]}"

  install_from_source BENCHMARK "${LIST_TARGETS[@]}" "${LIST_MINGW_TARGETS[@]}"

  # Install v8. v8 has better WASM SIMD support than NodeJS 14.
  # First we need the installer to install v8.
  npm install jsvu -g
  # install specific version;
  HOME=/opt jsvu --os=linux64 "v8@${V8_VERSION}"
  ln -s "/opt/.jsvu/v8-${V8_VERSION}" "/opt/.jsvu/v8"

  # Cleanup.
  find /var/lib/apt/lists/ -mindepth 1 -delete
}

main "$@"
