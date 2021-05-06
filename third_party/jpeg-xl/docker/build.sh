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

declare -a TARGETS

load_targets() {
  # Built-in OSX "find" does not support "-m".
  FIND=$(which "gfind" || which "find")
  for f in $(${FIND} -maxdepth 1 -name 'Dockerfile.*' | sort); do
    local target="${f#*Dockerfile.}"
    TARGETS+=("${target}")
  done
}

usage() {
    cat >&2 <<EOF
Use: $1 [targets]

Available targets:
  * all
EOF
  for target in "${TARGETS[@]}"; do
    echo "  * ${target}" >&2
  done
}

build_target() {
  local target="$1"

  local dockerfile="${MYDIR}/Dockerfile.${target}"
  # JPEG XL builder images are stored in the gcr.io/jpegxl project.
  local tag="gcr.io/jpegxl/${target}"

  echo "Building ${target}"
  if ! sudo docker build --no-cache -t "${tag}" -f "${dockerfile}" "${MYDIR}" \
      >"${target}.log" 2>&1; then
    echo "${target} failed. See ${target}.log" >&2
  else
    echo "Done, to upload image run:" >&2
    echo "  sudo docker push ${tag}"
    if [[ "${JPEGXL_PUSH:-}" == "1" ]]; then
      echo "sudo docker push ${tag}" >&2
      sudo docker push "${tag}"
      # The RepoDigest is only created after it is pushed.
      local fulltag=$(sudo docker inspect --format="{{.RepoDigests}}" "${tag}")
      fulltag="${fulltag#[}"
      fulltag="${fulltag%]}"
      echo "Updating .gitlab-ci.yml to ${fulltag}" >&2
      sed -E "s;${tag}@sha256:[0-9a-f]+;${fulltag};" \
        -i "${MYDIR}/../.gitlab-ci.yml"
    fi
  fi
}

main() {
  cd "${MYDIR}"
  local target="${1:-}"

  load_targets
  if [[ -z "${target}" ]]; then
    usage $0
    exit 1
  fi

  if [[ "${target}" == "all" ]]; then
    for target in "${TARGETS[@]}"; do
      build_target "${target}"
    done
  else
    for target in "$@"; do
      build_target "${target}"
    done
  fi
}

main "$@"
