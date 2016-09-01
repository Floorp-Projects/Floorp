#!/bin/sh

mkdir tmp/
git clone --no-checkout --depth 1 https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer tmp/
mv tmp/.git .
rm -Rf tmp
git reset --hard HEAD
