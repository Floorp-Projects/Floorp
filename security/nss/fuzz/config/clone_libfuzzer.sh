#!/bin/sh

LIBFUZZER_REVISION=8837e6cbbc842ab7524b06a2f7360c36add316b3

d=$(dirname $0)
$d/git-copy.sh https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer $LIBFUZZER_REVISION $d/../libFuzzer
