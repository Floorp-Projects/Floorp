#!/bin/bash -e

cd $SOURCE
TOP=$(cd ..; pwd)
export MOZBUILD_STATE_PATH=$TOP/mozbuild-state
[ -d $MOZBUILD_STATE_PATH ] || mkdir $MOZBUILD_STATE_PATH

export MOZCONFIG=$SOURCE/browser/config/mozconfigs/linux64/hazards

exec ./mach build -v -j8
