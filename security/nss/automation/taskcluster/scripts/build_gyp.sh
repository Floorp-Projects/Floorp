#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Clone NSPR if needed.
hg_clone https://hg.mozilla.org/projects/nspr ./nspr default

if [[ -f nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
  pushd nspr
  cat ../nss/nspr.patch | patch -p1
  popd
fi

# Dependencies
# For MacOS we have hardware in the CI which doesn't allow us o deploy VMs.
# The setup is hardcoded and can't be changed easily.
# This part is a helper We install dependencies manually to help.
if [ "$(uname)" = "Darwin" ]; then
  python3 -m pip install --user gyp-next
  python3 -m pip install --user ninja
  export PATH="$(python3 -m site --user-base)/bin:${PATH}"
fi

# Build.
nss/build.sh -g -v --enable-libpkix -Denable_draft_hpke=1 "$@"

# Package.
if [[ $(uname) = "Darwin" ]]; then
  mkdir -p public
  tar cvfjh public/dist.tar.bz2 dist
else
  mkdir artifacts
  tar cvfjh artifacts/dist.tar.bz2 dist
fi
