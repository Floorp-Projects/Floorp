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

TEST_DIRECTORY=$NSS_SOURCES_PATH/tests/acvp

git clone --depth=1 https://gitlab.com/nisec/nss-project/acvp-rust.git
cd acvp-rust
cargo build
TESTRUN="cargo run --bin test -- --profdata-command llvm-profdata-15"
echo "Big Number (fuzzed):"
$TESTRUN $TEST_DIRECTORY/fuzzed/bn.json bn nss
echo "AES-GCM (acvp-server):"
$TESTRUN $TEST_DIRECTORY/aes-gcm.json symmetric nss
echo "ECDSA (acvp-server):"
$TESTRUN $TEST_DIRECTORY/ecdsa.json ecdsa nss
echo "ECDSA (fuzzed):"
$TESTRUN $TEST_DIRECTORY/fuzzed/ecdsa.json ecdsa nss
echo "RSA (acvp-server):"
$TESTRUN $TEST_DIRECTORY/rsa.json rsa nss
echo "RSA (fuzzed):"
$TESTRUN $TEST_DIRECTORY/fuzzed/rsa.json rsa nss
echo "SHA-256 (acvp-server):"
$TESTRUN $TEST_DIRECTORY/sha256.json sha nss
$TESTRUN $TEST_DIRECTORY/sha256.mct.json sha nss
