#!/bin/bash -eu
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
################################################################################

# List of targets disabled for oss-fuzz.
declare -A disabled=([pkcs8]=1)

# List of targets we want to fuzz in TLS and non-TLS mode.
declare -A tls_targets=([tls-client]=1 [tls-server]=1)

# Helper function that copies a fuzzer binary and its seed corpus.
copy_fuzzer()
{
    local fuzzer=$1
    local name=$2

    # Copy the binary.
    cp ../dist/Debug/bin/$fuzzer $OUT/$name

    # Zip and copy the corpus, if any.
    if [ -d "$SRC/nss-corpus/$name" ]; then
        zip $OUT/${name}_seed_corpus.zip $SRC/nss-corpus/$name/*
    else
        zip $OUT/${name}_seed_corpus.zip $SRC/nss-corpus/*/*
    fi
}

# Copy libFuzzer options
cp fuzz/options/*.options $OUT/

# Build the library (non-TLS fuzzing mode).
CXX="$CXX -stdlib=libc++" LDFLAGS="$CFLAGS" \
    ./build.sh -c -v --fuzz=oss --fuzz --disable-tests

# Copy fuzzing targets.
for fuzzer in $(find ../dist/Debug/bin -name "nssfuzz-*" -printf "%f\n"); do
    name=${fuzzer:8}
    if [ -z "${disabled[$name]:-}" ]; then
        [ -n "${tls_targets[$name]:-}" ] && name="${name}-no_fuzzer_mode"
        copy_fuzzer $fuzzer $name
    fi
done

# Build the library again (TLS fuzzing mode).
CXX="$CXX -stdlib=libc++" LDFLAGS="$CFLAGS" \
    ./build.sh -c -v --fuzz=oss --fuzz=tls --disable-tests

# Copy dual mode targets in TLS mode.
for name in "${!tls_targets[@]}"; do
    if [ -z "${disabled[$name]:-}" ]; then
        copy_fuzzer nssfuzz-$name $name
    fi
done
