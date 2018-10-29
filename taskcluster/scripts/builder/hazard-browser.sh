#!/bin/bash -e

cd $SOURCE
TOP=$(cd ..; pwd)
export MOZBUILD_STATE_PATH=$TOP/mozbuild-state
[ -d $MOZBUILD_STATE_PATH ] || mkdir $MOZBUILD_STATE_PATH

export MOZCONFIG=$SOURCE/js/src/devtools/rootAnalysis/mozconfig.haz

exec ./mach build -v -j8
