#!/bin/bash
#
# Copyright 2019 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euox pipefail

if [[ -z ${ABSEIL_ROOT:-} ]]; then
  ABSEIL_ROOT="$(realpath $(dirname ${0})/..)"
fi

source "${ABSEIL_ROOT}/ci/linux_docker_containers.sh"
readonly DOCKER_CONTAINER=${LINUX_GCC_LATEST_CONTAINER}

time docker run \
    --volume="${ABSEIL_ROOT}:/abseil-cpp:ro" \
    --workdir=/abseil-cpp \
    --tmpfs=/buildfs:exec \
    --cap-add=SYS_PTRACE \
    --rm \
    -e CFLAGS="-Werror" \
    -e CXXFLAGS="-Werror" \
    ${DOCKER_CONTAINER} \
    /bin/bash CMake/install_test_project/test.sh $@
