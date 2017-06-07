#!/bin/sh

LIBFUZZER_REVISION=56bd1d43451cca4b6a11d3be316bb77ab159b09d

d=$(dirname $0)
$d/git-copy.sh https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer $LIBFUZZER_REVISION $d/../libFuzzer
