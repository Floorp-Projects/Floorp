#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Clone NSPR if needed.
if [ ! -d "nspr" ]; then
    hg_clone https://hg.mozilla.org/projects/nspr ./nspr default
fi

# Build and run Coverity
cd nss
./mach static-analysis

# Return the exit code of the Coverity Analysis
exit $?