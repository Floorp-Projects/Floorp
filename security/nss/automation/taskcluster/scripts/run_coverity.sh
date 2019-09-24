#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Clone NSPR if needed.
if [ ! -d "nspr" ]; then
    hg_clone https://hg.mozilla.org/projects/nspr ./nspr default

  if [[ -f nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
    pushd nspr
    cat ../nss/nspr.patch | patch -p1
    popd
  fi
fi

# Build and run Coverity
cd nss
./mach static-analysis

# Return the exit code of the Coverity Analysis
exit $?
