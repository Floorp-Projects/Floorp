#!/bin/bash -e

# Optionally get revision from cmd line
[ $1 ] && REVISION=$1 || REVISION=c815210013f27cfac07d6b53b47e8ac53e86afa3

mkdir tmp
git clone --single-branch --no-checkout --shallow-since "2019-03-01" https://github.com/llvm/llvm-project tmp

(cd tmp && git reset --hard $REVISION)

# libFuzzer source files
CPPS=($(ls -rv tmp/compiler-rt/lib/fuzzer/*.cpp))
CPPS=(${CPPS[@]##*/})
CPPS=(${CPPS[@]##FuzzerMain*}) # ignored

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
