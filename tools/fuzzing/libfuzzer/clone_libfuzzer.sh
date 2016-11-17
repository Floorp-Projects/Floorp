#!/bin/sh

mkdir tmp/
git clone --no-checkout --depth 1 https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer tmp/
(cd tmp && git reset --hard 61f251f8f4eef0ec6fae92bd0d7b63e76ed75b7f)

# Copy only source code and includes
cp tmp/*.cpp tmp/*.h tmp/*.def .

# Remove the temporary directory
rm -Rf tmp/
