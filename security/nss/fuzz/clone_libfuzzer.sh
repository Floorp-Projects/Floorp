#!/bin/sh

cd $(dirname $0)
mkdir tmp/
git clone -q https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer tmp/
mv tmp/.git libFuzzer/
rm -fr tmp
cd libFuzzer
git reset --hard 4333f2ca71eb7951fcafcdcb111012fbe25c5e7e
