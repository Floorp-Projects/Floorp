#!/bin/bash

set -ex

mkdir -p /builds/worker/artifacts/
mkdir -p /builds/worker/.local/state/snapcraft/
ln -s /builds/worker/artifacts /builds/worker/.local/state/snapcraft/log

BRANCH=$1
DEBUG=${2:-0}

export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export SNAP_ARCH=amd64
export SNAPCRAFT_BUILD_INFO=1

export PATH=$PATH:$HOME/.local/bin/
unset MOZ_AUTOMATION

MOZCONFIG=mozconfig.in

USE_SNAP_FROM_STORE_OR_MC=${USE_SNAP_FROM_STORE_OR_MC:-0}

TRY=0
if [ "${BRANCH}" = "try" ]; then
  BRANCH=nightly
  TRY=1
fi

if [ "${USE_SNAP_FROM_STORE_OR_MC}" = "0" ]; then
  # ESR currently still has a hard dependency against zstandard==0.17.0 so
  # install this specific version here
  if [ "${BRANCH}" = "esr" ]; then
    sudo apt-get remove -y python3-zstandard && sudo apt-get install -y python3-pip && sudo pip3 install --no-input zstandard==0.17.0
    MOZCONFIG=mozconfig
  fi

  # Stable and beta runs out of file descriptors during link with gold
  ulimit -n 65536

  git clone --single-branch --depth 1 --branch "${BRANCH}" https://github.com/canonical/firefox-snap/
  cd firefox-snap/

  if [ "${TRY}" = "1" ]; then
    # Symlink so that we can directly re-use Gecko mercurial checkout
    ln -s /builds/worker/checkouts/gecko gecko
  fi

  # Force an update to avoid the case of a stale docker image and repos updated
  # after
  sudo apt-get update

  # shellcheck disable=SC2046
  sudo apt-get install -y $(/usr/bin/python3 /builds/worker/parse.py snapcraft.yaml)

  # CRAFT_PARTS_PACKAGE_REFRESH required to avoid snapcraft running apt-get update
  # especially for stage-packages
  if [ -d "/builds/worker/patches/${BRANCH}/" ]; then
    for p in /builds/worker/patches/"${BRANCH}"/*.patch; do
      patch -p1 < "$p"
    done;
  fi

  if [ "${TRY}" = "1" ]; then
    # don't remove hg source, and don't force changeset so we get correct stamp
    # still force repo because the try clone is from mozilla-unified but the
    # generated link does not work
    sed -ri 's|rm -rf .hg||g' snapcraft.yaml
    # shellcheck disable=SC2016
    sed -ri 's|MOZ_SOURCE_REPO=\$\{REPO\}|MOZ_SOURCE_REPO=${GECKO_HEAD_REPOSITORY}|g' snapcraft.yaml
    # shellcheck disable=SC2016
    sed -ri 's|MOZ_SOURCE_CHANGESET=\$\{REVISION\}|MOZ_SOURCE_CHANGESET=${GECKO_HEAD_REV}|g' snapcraft.yaml
    # shellcheck disable=SC2016
    sed -ri 's|hg clone --stream \$REPO -u \$REVISION|cp -r \$SNAPCRAFT_PROJECT_DIR/gecko/. |g' snapcraft.yaml
  fi

  if [ "${DEBUG}" = "1" ]; then
    {
      echo "ac_add_options --enable-debug"
      echo "ac_add_options --disable-install-strip"
    } >> ${MOZCONFIG}
    echo "MOZ_DEBUG=1" >> ${MOZCONFIG}

    # No PGO on debug builds
    sed -ri 's/ac_add_options --enable-linker=gold//g' snapcraft.yaml
    sed -ri 's/ac_add_options --enable-lto=cross//g' snapcraft.yaml
    sed -ri 's/ac_add_options MOZ_PGO=1//g' snapcraft.yaml
  fi

  SNAPCRAFT_BUILD_ENVIRONMENT_MEMORY=64G \
  SNAPCRAFT_BUILD_ENVIRONMENT_CPU=$(nproc) \
  CRAFT_PARTS_PACKAGE_REFRESH=0 \
    snapcraft --destructive-mode --verbose
elif [ "${USE_SNAP_FROM_STORE_OR_MC}" = "store" ]; then
  mkdir from-snap-store && cd from-snap-store

  CHANNEL="${BRANCH}"
  if [ "${CHANNEL}" = "try" ] || [ "${CHANNEL}" = "nightly" ]; then
    CHANNEL=edge
  fi;

  snap download --channel="${CHANNEL}" firefox
  SNAP_DEBUG_NAME=$(find . -maxdepth 1 -type f -name "firefox*.snap" | sed -e 's/\.snap$/.debug/g')
  touch "${SNAP_DEBUG_NAME}"
else
  mkdir from-mc && cd from-mc

  # index.gecko.v2.mozilla-central.latest.firefox.amd64-esr-debug
  #  => https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.latest.firefox.amd64-esr-debug/artifacts/public%2Fbuild%2Ffirefox.snap
  # index.gecko.v2.mozilla-central.revision.bf0897ec442e625c185407cc615a6adc0e40fa75.firefox.amd64-esr-debug
  #  => https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.revision.bf0897ec442e625c185407cc615a6adc0e40fa75.firefox.amd64-esr-debug/artifacts/public%2Fbuild%2Ffirefox.snap
  # index.gecko.v2.mozilla-central.latest.firefox.amd64-nightly
  #  => https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.latest.firefox.amd64-nightly/artifacts/public%2Fbuild%2Ffirefox.snap
  # index.gecko.v2.mozilla-central.revision.bf0897ec442e625c185407cc615a6adc0e40fa75.firefox.amd64-nightly
  #  => https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.revision.bf0897ec442e625c185407cc615a6adc0e40fa75.firefox.amd64-nightly/artifacts/public%2Fbuild%2Ffirefox.snap

  INDEX_NAME="${BRANCH}"
  if [ "${INDEX_NAME}" = "try" ]; then
    INDEX_NAME=nightly
  fi;

  if [ "${DEBUG}" = "1" ]; then
    INDEX_NAME="${INDEX_NAME}-debug"
  fi;

  TASKCLUSTER_API_ROOT="https://firefox-ci-tc.services.mozilla.com/api"

  URL_TASK="${TASKCLUSTER_API_ROOT}/index/v1/task/gecko.v2.mozilla-central.${USE_SNAP_FROM_STORE_OR_MC}.firefox.amd64-${INDEX_NAME}"
  PKGS_TASK_ID=$(curl "${URL_TASK}" | jq -r '.taskId')

  if [ -z "${PKGS_TASK_ID}" ]; then
    echo "Failure to find matching taskId for ${USE_SNAP_FROM_STORE_OR_MC} + ${INDEX_NAME}"
    exit 1
  fi

  PKGS_URL="${TASKCLUSTER_API_ROOT}/queue/v1/task/${PKGS_TASK_ID}/artifacts"
  for pkg in $(curl "${PKGS_URL}" | jq -r '.artifacts | . [] | select(.name | contains("public/build/firefox_")) | .name');
  do
    url="${TASKCLUSTER_API_ROOT}/queue/v1/task/${PKGS_TASK_ID}/artifacts/${pkg}"
    target_name="$(basename "${pkg}")"
    echo "$url => $target_name"
    curl -SL "${url}" -o "${target_name}"
  done;
fi

cp ./*.snap ./*.debug /builds/worker/artifacts/

# Those are for fetches usage by the test task
cp ./*.snap /builds/worker/artifacts/firefox.snap
cp ./*.debug /builds/worker/artifacts/firefox.debug

# Those are for running snap-upstream-test
cd /builds/worker/checkouts/gecko/taskcluster/docker/snap-coreXX-build/snap-tests/ && zip -r9 /builds/worker/artifacts/snap-tests.zip ./*
