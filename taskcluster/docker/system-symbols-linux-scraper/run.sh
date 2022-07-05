#!/bin/bash

set -ex

SHA256SUMS=SHA256SUMS.zip

function get_route()
{
  local task_url=${TASKCLUSTER_ROOT_URL}/api/queue/v1/task/${TASK_ID}
  local payload
  payload=$(curl -sSL "${task_url}")

  local route
  route=$(echo "${payload}" | jq -r '.routes[] | select(contains("latest")) | select(contains("pushdate") | not) ' | sed -e 's/^index\.//')
  echo "${route}"
}

function get_sha256sum_url()
{
  local route
  route=$(get_route)
  local sha256sums_url=${TASKCLUSTER_ROOT_URL}/api/index/v1/task/${route}/artifacts/public/build/${SHA256SUMS}
  echo "${sha256sums_url}"
}

function has_sha256sums_on_index()
{
  local url
  url=$(get_sha256sum_url)
  curl -sSL --head --fail -o /dev/null "${url}"
}

function download_verify_extract_sha256sums()
{
  local url=$1

  if [ ! -f "/builds/worker/SHA256SUMS.txt" ]; then
    echo "Missing checksums, aborting."
    exit 1
  fi

  curl -sSL "${url}" -o ${SHA256SUMS}

  # We verify SHA256SUMS when we bootstrap and thus download from GitHub
  # The one downloaded from TaskCluster will have been updated by previous tasks
  if [ -n "${BOOTSTRAP_SHA256}" ]; then
    echo "BOOTSTRAP_SHA256 was set, ensuring consistent set of SHA256SUMS"
    local sha_check
    sha256sum --quiet --status -c <(grep "${DISTRO}/" /builds/worker/SHA256SUMS.txt | sed -e "s/${DISTRO}\///")
    sha_check=$?
    if [ ${sha_check} -ne 0 ]; then
      echo "Checksum verification failed, aborting."
      exit 1
    fi
  fi

  unzip ${SHA256SUMS} && rm ${SHA256SUMS}
}

DISTRO=$1

sha256=https://mozilla.github.io/symbol-scrapers/${DISTRO}/${SHA256SUMS}
if [ -z "${BOOTSTRAP_SHA256}" ]; then
  if has_sha256sums_on_index; then
    sha256=$(get_sha256sum_url)
  fi
fi

mkdir -p /builds/worker/artifacts/

pushd "${MOZ_FETCHES_DIR}/symbol-scrapers/${DISTRO}"
  download_verify_extract_sha256sums "${sha256}"
  DUMP_SYMS=${MOZ_FETCHES_DIR}/dump_syms/dump_syms /bin/bash script.sh
  zip -r9 /builds/worker/artifacts/${SHA256SUMS} SHA256SUMS
  cp wget*.log /builds/worker/artifacts/
popd

if [ ! -f "/builds/worker/artifacts/target.crashreporter-symbols.zip" ]; then
  echo "No symbols zip produced, upload task will fail"
fi
