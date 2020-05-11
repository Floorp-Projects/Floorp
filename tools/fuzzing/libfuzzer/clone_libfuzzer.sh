#!/bin/bash

# Optionally get revision from cmd line
# Changelog: https://reviews.llvm.org/source/compiler-rt/history/compiler-rt/trunk/lib/fuzzer/
[ $1 ] && REVISION=$1 || REVISION=356365

mkdir tmp
svn co -qr $REVISION https://llvm.org/svn/llvm-project/compiler-rt/trunk tmp || exit

if [ $1 ]; then
  # libFuzzer source files
  CPPS=($(ls -rv tmp/lib/fuzzer/*.cpp))
  CPPS=(${CPPS[@]##*/})
  CPPS=(${CPPS[@]##FuzzerMain*}) # ignored

  # Update SOURCES entries
  sed -e "/^SOURCES/,/^]/ {/'/d}" -i moz.build
  for CPP in ${CPPS[@]}; do sed -e "/^SOURCES/ a \\\t'${CPP}'," -i moz.build; done

  # Remove previous files
  rm *.{cpp,h,def}
fi

# Copy files
cp tmp/lib/fuzzer/*.{cpp,h,def} .

# Remove the temporary directory
rm -Rf tmp/

[ $1 ] && echo "Updated libFuzzer to ${REVISION}"

