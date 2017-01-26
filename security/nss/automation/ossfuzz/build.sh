#!/bin/bash -eu
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
################################################################################

# List of targets disabled for oss-fuzz.
declare -A disabled=([pkcs8]=1)

# Build the library.
CXX="$CXX -stdlib=libc++" LDFLAGS="$CFLAGS" \
    ./build.sh -c -v --fuzz=oss --fuzz=tls --disable-tests

# Find fuzzing targets.
for fuzzer in $(find ../dist/Debug/bin -name "nssfuzz-*" -printf "%f\n"); do
    name=${fuzzer:8}
    [ -n "${disabled[$name]:-}" ] && continue;

    # Copy the binary.
    cp ../dist/Debug/bin/$fuzzer $OUT/$name

    # Zip and copy the corpus, if any.
    if [ -d "$SRC/nss-corpus/$name" ]; then
        zip $OUT/${name}_seed_corpus.zip $SRC/nss-corpus/$name/*
    else
        zip $OUT/${name}_seed_corpus.zip $SRC/nss-corpus/*/*
    fi
done
