#!/bin/sh

mkdir tmp/
git clone --no-checkout --depth 1 https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer tmp/
(cd tmp && git reset --hard f74d9f33e526fff0e8d17f08bb0e5982a821f70e)

# Copy only source code and includes
cp tmp/*.cpp tmp/*.h tmp/*.def .

# Remove the temporary directory
rm -Rf tmp/
