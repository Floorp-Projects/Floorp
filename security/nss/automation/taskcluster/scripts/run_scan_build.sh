#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) = 0 ]; then
    source $(dirname $0)/tools.sh

    # Set compiler.
    switch_compilers

    # Drop privileges by re-running this script.
    exec su worker $0 $@
fi

# Clone NSPR if needed.
if [ ! -d "nspr" ]; then
    hg clone https://hg.mozilla.org/projects/nspr
fi

# Build.
cd nss && make nss_build_all

# we run scan-build on these folders
declare -a scan=("lib/ssl" "lib/freebl")

for i in "${scan[@]}"
do
   echo "cleaning $i ..."
   find "$i" -name "*.OBJ" | xargs rm -fr
done

# run scan-build
scan-build -o /home/worker/artifacts/ make nss_build_all && cd ..

# print errors we found
set +v +x
for i in "${scan[@]}"
do
   n=$(grep -Rn "${i#*/}/" /home/worker/artifacts/*/index.html | wc -l)
   # TODO: print FAILED/PASSED and set exit code for folders we expect to be clean
   echo "$(date '+%T') WARNING - TEST-UNEXPECTED-FAIL: $i contains $n scan-build errors"
done
