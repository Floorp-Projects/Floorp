#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# This file downloads the dependencies needed to build JPEG XL into third_party.
# These dependencies are normally pulled by gtest.

set -eu

MYDIR=$(dirname $(realpath "$0"))

# Git revisions we use for the given submodules. Update these whenever you
# update a git submodule.
THIRD_PARTY_BROTLI="35ef5c554d888bef217d449346067de05e269b30"
THIRD_PARTY_HIGHWAY="22e3d7276f4157d4a47586ba9fd91dd6303f441a"
THIRD_PARTY_SKCMS="64374756e03700d649f897dbd98c95e78c30c7da"
THIRD_PARTY_SJPEG="868ab558fad70fcbe8863ba4e85179eeb81cc840"
THIRD_PARTY_ZLIB="cacf7f1d4e3d44d871b605da3b647f07d718623f"
THIRD_PARTY_LIBPNG="a40189cf881e9f0db80511c382292a5604c3c3d1"

# Download the target revision from GitHub.
download_github() {
  local path="$1"
  local project="$2"

  local varname=`echo "$path" | tr '[:lower:]' '[:upper:]'`
  varname="${varname/\//_}"
  local sha
  eval "sha=\${${varname}}"

  local down_dir="${MYDIR}/downloads"
  local local_fn="${down_dir}/${sha}.tar.gz"
  if [[ -e "${local_fn}" && -d "${MYDIR}/${path}" ]]; then
    echo "${path} already up to date." >&2
    return 0
  fi

  local url
  local strip_components=0
  if [[ "${project:0:4}" == "http" ]]; then
    # "project" is a googlesource.com base url.
    url="${project}${sha}.tar.gz"
  else
    # GitHub files have a top-level directory
    strip_components=1
    url="https://github.com/${project}/tarball/${sha}"
  fi

  echo "Downloading ${path} version ${sha}..." >&2
  mkdir -p "${down_dir}"
  curl -L --show-error -o "${local_fn}.tmp" "${url}"
  mkdir -p "${MYDIR}/${path}"
  tar -zxf "${local_fn}.tmp" -C "${MYDIR}/${path}" \
    --strip-components="${strip_components}"
  mv "${local_fn}.tmp" "${local_fn}"
}


main() {
  if git -C "${MYDIR}" rev-parse; then
    cat >&2 <<EOF
Current directory is a git repository, downloading dependencies via git:

  git submodule update --init --recursive

EOF
    git -C "${MYDIR}" submodule update --init --recursive --depth 1 --recommend-shallow
    return 0
  fi

  # Sources downloaded from a tarball.
  download_github third_party/brotli google/brotli
  download_github third_party/highway google/highway
  download_github third_party/sjpeg webmproject/sjpeg
  download_github third_party/skcms \
    "https://skia.googlesource.com/skcms/+archive/"
  download_github third_party/zlib madler/zlib
  download_github third_party/libpng glennrp/libpng
  echo "Done."
}

main "$@"
