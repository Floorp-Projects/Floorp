#!/bin/sh

cd $(dirname $0)

mkdir tmp/
git clone --no-checkout --depth 1 https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer tmp/
(cd tmp && git reset --hard 1b543d6e5073b56be214394890c9193979a3d7e1)

mkdir -p libFuzzer
cp tmp/*.cpp tmp/*.h tmp/*.def libFuzzer
rm -Rf tmp/
