#!/bin/bash -e

# Optionally get revision from cmd line
[ $1 ] && REVISION=$1 || REVISION=76d07503f0c69f6632e6d8d4736e2a4cb4055a92

mkdir tmp
git clone --single-branch --no-checkout --shallow-since "2020-07-01" https://github.com/llvm/llvm-project tmp

(cd tmp && git reset --hard $REVISION)

# libFuzzer source files
CPPS=($(ls tmp/compiler-rt/lib/fuzzer/*.cpp | sort -r))
CPPS=(${CPPS[@]##*/})
CPPS=(${CPPS[@]##FuzzerMain*}) # ignored
CPPS=(${CPPS[@]##FuzzerInterceptors*}) # ignored

# Update SOURCES entries
sed -e "/^SOURCES/,/^]/ {/'/d}" -i moz.build
for CPP in ${CPPS[@]}; do sed -e "/^SOURCES/ a \\    '${CPP}'," -i moz.build; done

# Remove previous files
rm *.{cpp,h,def}

# Copy files
cp tmp/compiler-rt/lib/fuzzer/*.{cpp,h,def} .

# Apply local patches
for patch in patches/*.patch
do
  patch -p4 < $patch
done

# Remove the temporary directory
rm -Rf tmp/

echo "Updated libFuzzer to ${REVISION}"
