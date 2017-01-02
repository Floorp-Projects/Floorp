#!/bin/sh

d=$(dirname $0)
exec $d/git-copy.sh https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer 1b543d6e5073b56be214394890c9193979a3d7e1 $d/libFuzzer
