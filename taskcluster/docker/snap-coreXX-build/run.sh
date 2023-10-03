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

# ESR currently still has a hard dependency against zstandard==0.17.0 so
# install this specific version here
if [ "${BRANCH}" = "esr" ]; then
  sudo apt-get remove -y python3-zstandard && sudo apt-get install -y python3-pip && sudo pip3 install --no-input zstandard==0.17.0
  MOZCONFIG=mozconfig
fi

# Stable and beta runs out of file descriptors during link with gold
ulimit -n 65536

TRY=0
if [ "${BRANCH}" = "try" ]; then
  BRANCH=nightly
  TRY=1
fi

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
cp ./*.snap ./*.debug /builds/worker/artifacts/

# Those are for fetches usage by the test task
cp ./*.snap /builds/worker/artifacts/firefox.snap
cp ./*.debug /builds/worker/artifacts/firefox.debug

# Those are for running snap-upstream-test
cd /builds/worker/checkouts/gecko/taskcluster/docker/snap-coreXX-build/snap-tests/ && zip -r9 /builds/worker/artifacts/snap-tests.zip ./*
