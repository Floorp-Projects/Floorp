#!/bin/sh

LIBFUZZER_REVISION=6937e68f927b6aefe526fcb9db8953f497e6e74d

d=$(dirname $0)
$d/git-copy.sh https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer $LIBFUZZER_REVISION $d/../libFuzzer
