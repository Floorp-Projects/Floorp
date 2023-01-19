#!/bin/bash -eu
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
################################################################################
export NSS_PATH=$PWD NSS_SOURCES_PATH=$PWD/nss
export LD_LIBRARY_PATH=$PWD/dist/Debug/lib/
export RUST_LOG=warn
export RUSTFLAGS="-C instrument-coverage"
cd nss
CC=clang-15 CXX=clang++-15 ./build.sh -g -v --sourcecov --static --disable-tests

git clone --depth=1 https://gitlab.com/nisec/nss-project/acvp-rust.git
cd acvp-rust
cargo build
TESTRUN="cargo run --bin test -- --profdata-command llvm-profdata-15"
echo "AES-GCM:"
$TESTRUN acvp-rust/samples/aes-gcm.json symmetric nss
echo "ECDSA:"
$TESTRUN acvp-rust/samples/ecdsa.json ecdsa nss
echo "RSA:"
$TESTRUN acvp-rust/samples/rsa.json rsa nss
echo "SHA-256:"
$TESTRUN acvp-rust/samples/sha256.json sha nss
