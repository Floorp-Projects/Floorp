#!/bin/bash -ex

# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH

WORKSPACE=$( cd "$1" && pwd )

export GECKO_DIR="$WORKSPACE/gecko"
export TOOLTOOL_DIR="$WORKSPACE"
export MOZ_OBJDIR="$WORKSPACE/obj-analyzed"

mkdir -p "$MOZ_OBJDIR"

install-packages.sh "$WORKSPACE"

. hazard-analysis.sh
. setup-ccache.sh

build_js_shell

# Artifacts folder is outside of the cache.
mkdir -p $HOME/artifacts/ || true

function onexit () {
    grab_artifacts "$WORKSPACE/analysis" "$HOME/artifacts"
}

trap onexit EXIT

configure_analysis "$WORKSPACE/analysis"
run_analysis "$WORKSPACE/analysis" shell

check_hazards "$WORKSPACE/analysis"

################################### script end ###################################
