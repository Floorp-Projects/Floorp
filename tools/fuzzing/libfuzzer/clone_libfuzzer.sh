#!/bin/sh

mkdir tmp/
git clone --no-checkout --depth 1 https://chromium.googlesource.com/chromium/llvm-project/compiler-rt/lib/fuzzer tmp/
(cd tmp && git reset --hard 2c1f00d30409ff8662e62a6480718726dad77586)

# Copy only source code and includes
cp tmp/*.cpp tmp/*.h tmp/*.def .

# Remove the temporary directory
rm -Rf tmp/
