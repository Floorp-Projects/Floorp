#!/bin/bash
set -x -e -v

# This script runs the windows CI scripts for standalone WebRender. The CI
# scripts build WebRender in various "standalone" (without Gecko)
# configurations and also run WebRender's reftest suite using the `wrench`
# tool in the WebRender repository.
# The builds involved require a number of dependencies to be available,
# which is all handled below.

cd $GECKO_PATH

# This will download the rustc, cmake, ninja, MSVC, and wrench-deps artifacts.
WORKSPACE="$PWD/../../" taskcluster/scripts/misc/tooltool-download.sh
export PATH=$PATH:$PWD/rustc/bin:$PWD/cmake/bin:$PWD/ninja/bin

# We will be sourcing mozconfig files, which end up calling mk_add_options with
# various settings. We only need the variable settings they create along the
# way. Define mk_add_options as empty so the variables are defined but nothing
# else happens.
# This is adapted from the start of js/src/devtools/automation/winbuildenv.sh
# which does a similar thing.
mk_add_options() {
    :
}

export topsrcdir=$PWD
. $topsrcdir/build/win64/mozconfig.vs2017

win_sdk_version="10.0.17134.0"
export PATH="${VSPATH}/VC/bin/Hostx64/x64:${VSPATH}/SDK/bin/${win_sdk_version}/x64:${PATH}"
export INCLUDE="${VSPATH}/VC/include:${VSPATH}/VC/atlmfc/include:${VSPATH}/SDK/Include/${win_sdk_version}/ucrt:${VSPATH}/SDK/Include/${win_sdk_version}/shared:${VSPATH}/SDK/Include/${win_sdk_version}/um:${VSPATH}/SDK/Include/${win_sdk_version}/winrt"
export LIB="${VSPATH}/VC/lib/x64:${VSPATH}/VC/atlmfc/lib/x64:${VSPATH}/SDK/Lib/${win_sdk_version}/ucrt/x64:${VSPATH}/SDK/Lib/${win_sdk_version}/um/x64"

# In the msys bash that we're running in, environment variables containing paths
# will *sometimes* get converted to windows format on their own, but it doesn't
# happen consistently. See bug 1508828 around comment 9 onwards for some details.
# To ensure we consistently convert the INCLUDE and LIB variables that MSVC
# needs for building stuff, we do it here explicitly.

convert_path_to_win() {
    echo $1 |
    sed -e 's#:#\n#g' |           # split path components into separate lines
    sed -e 's#^/\(.\)/#\1:/#' |   # replace first path component (if one letter) with equivalent drive letter
    sed -e 's#/#\\#g' |           # convert all forward slashes to backslashes
    paste -s -d ';'               # glue the lines back together
}

export INCLUDE=$(convert_path_to_win "$INCLUDE")
export LIB=$(convert_path_to_win "$LIB")

# Move the wrench-deps vendored crates into place
mv wrench-deps/{vendor,.cargo} gfx/wr
cd gfx/wr

# This is needed for the WebRender standalone reftests
powershell.exe 'iex (Get-Content -Raw ci-scripts\set-screenresolution.ps1); Set-ScreenResolution 1920 1080'

# Run the CI scripts
export CARGOFLAGS='--verbose --frozen'
export FREETYPE_CMAKE_GENERATOR=Ninja
cmd.exe /c 'ci-scripts\windows-tests.cmd'
