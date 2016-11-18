#!/bin/sh

cd $(dirname $0)
mkdir tmp/
git clone -q https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer tmp/
mv tmp/.git libFuzzer/
rm -fr tmp
cd libFuzzer
git reset --hard 1b543d6e5073b56be214394890c9193979a3d7e1
