#!/bin/sh
##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##

if ! [ -x "$(command -v emcc)" ] \
    || ! [ -x "$(command -v emconfigure)" ] \
    || ! [ -x "$(command -v emmake)" ]; then
  cat << EOF >& 2
Emscripten SDK is not available (emcc, emconfigure or emmake is missing).
Install it from
https://kripken.github.io/emscripten-site/docs/getting_started/downloads.html
and try again.
EOF
  exit 1
fi

echo 'Building JS Inspector'
if [ ! -d ".inspect" ]; then
  mkdir .inspect
  cd .inspect && emconfigure ../../configure \
    --disable-multithread \
    --disable-runtime-cpu-detect \
    --target=generic-gnu \
    --enable-accounting \
    --disable-docs \
    --disable-unit-tests \
    --enable-inspection \
    --enable-highbitdepth \
    --extra-cflags="-D_POSIX_SOURCE"
  cd ..
fi

cd .inspect
emmake make -j 8
cp examples/inspect inspect.bc
emcc -O3 inspect.bc -o inspect.js \
  -s TOTAL_MEMORY=134217728 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME="'DecoderModule'" \
  --post-js "../inspect-post.js" \
  --memory-init-file 0
cp inspect.js ../inspect.js
