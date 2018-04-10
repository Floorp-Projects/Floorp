#!/bin/sh

mkdir tmp/
git clone --no-checkout --depth 1 https://chromium.googlesource.com/chromium/llvm-project/compiler-rt/lib/fuzzer tmp/
(cd tmp && git reset --hard c2b235ee789fd452ba37c57957cc280fb37f9c52)

# Copy only source code and includes
cp tmp/*.cpp tmp/*.h tmp/*.def .

# Remove the temporary directory
rm -Rf tmp/
