#!/bin/bash

set -ex

SHA256SUMS="SHA256SUMS.zip"

function get_route()
{
  local task_url="${TASKCLUSTER_ROOT_URL}/api/queue/v1/task/${TASK_ID}"
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
  local sha256sums_url="${TASKCLUSTER_ROOT_URL}/api/index/v1/task/${route}/artifacts/public/build/${SHA256SUMS}"
  echo "${sha256sums_url}"
}

function has_sha256sums_on_index()
{
  local url
  url=$(get_sha256sum_url)
  curl -sSL --head --fail -o /dev/null "${url}"
}

function download_extract_sha256sums()
{
  local url=$1
  curl -sSL "${url}" -o "${SHA256SUMS}"
  unzip "${SHA256SUMS}" && rm "${SHA256SUMS}"
}

if has_sha256sums_on_index; then
  sha256=$(get_sha256sum_url)
fi

mkdir -p /builds/worker/artifacts/

pushd "${MOZ_FETCHES_DIR}/symbol-scrapers/windows-graphics-drivers"
  if [ -z "${sha256}" ]; then
    touch SHA256SUMS # First run, create the file
  else
    download_extract_sha256sums "${sha256}"
  fi

  DUMP_SYMS="${MOZ_FETCHES_DIR}/dump_syms/dump_syms" /bin/bash -x script.sh
  zip -r9 "/builds/worker/artifacts/${SHA256SUMS}" SHA256SUMS
popd

if [ ! -f "/builds/worker/artifacts/target.crashreporter-symbols.zip" ]; then
  echo "No symbols zip produced, upload task will fail"
fi
