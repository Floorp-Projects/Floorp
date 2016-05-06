#!/bin/bash -ex

################################### build-mulet-haz-linux.sh ###################################
# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH
WORKSPACE=$( cd "$1" && pwd )
export TOOLTOOL_DIR="$WORKSPACE"

. desktop-setup.sh
. hazard-analysis.sh

build_js_shell

# Artifacts folder is outside of the cache.
mkdir -p $HOME/artifacts/ || true

function onexit () {
    grab_artifacts "$WORKSPACE/analysis" "$HOME/artifacts"
}

trap onexit EXIT

configure_analysis "$WORKSPACE/analysis"
run_analysis "$WORKSPACE/analysis" browser

check_hazards "$WORKSPACE/analysis"

################################### script end ###################################
