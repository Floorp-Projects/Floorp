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

configure_analysis "$WORKSPACE/analysis"
run_analysis "$WORKSPACE/analysis" b2g

# Artifacts folder is outside of the cache.
mkdir -p $HOME/artifacts/ || true

grab_artifacts "$WORKSPACE/analysis" "$HOME/artifacts"
check_hazards "$WORKSPACE/analysis"

################################### script end ###################################
