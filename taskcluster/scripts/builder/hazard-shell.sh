#!/bin/bash -ex

mkdir -p "$ANALYZED_OBJDIR"
cd "$ANALYZED_OBJDIR"
$SOURCE/js/src/configure --enable-debug --enable-optimize --enable-ctypes --enable-nspr-build
make -j8 -s
