#!/bin/bash
set -x -e -v

# This script runs the windows CI scripts for standalone WebRender. The CI
# scripts build WebRender in various "standalone" (without Gecko)
# configurations and also run WebRender's reftest suite using the `wrench`
# tool in the WebRender repository.
# The builds involved require a number of dependencies to be available,
# which is all handled below.

cd $GECKO_PATH

# This will download the rustc, MSVC, and wrench-deps artifacts.
. taskcluster/scripts/misc/tooltool-download.sh
export PATH=$PATH:$MOZ_FETCHES_DIR/rustc/bin

.  taskcluster/scripts/misc/vs-setup.sh

# Move the wrench-deps vendored crates into place
mv ${MOZ_FETCHES_DIR}/wrench-deps/{vendor,.cargo} gfx/wr
cd gfx/wr

# This is needed for the WebRender standalone reftests
powershell.exe 'iex (Get-Content -Raw ci-scripts\set-screenresolution.ps1); Set-ScreenResolution 1920 1080'

# Run the CI scripts
export CARGOFLAGS='--verbose --frozen'
cmd.exe /c 'ci-scripts\windows-tests.cmd'

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
