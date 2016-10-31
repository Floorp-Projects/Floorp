#!/usr/bin/env bash

source $(dirname $0)/tools.sh

if [ $(id -u) = 0 ]; then
    # Drop privileges by re-running this script.
    exec su worker $0 $@
fi

# Clone NSPR if needed.
if [ ! -d "nspr" ]; then
    hg_clone https://hg.mozilla.org/projects/nspr nspr default
fi

# Build.
cd nss
make nss_build_all

# What we want to scan.
# key: directory to scan
# value: number of errors expected in that directory
declare -A scan=( \
        [lib/base]=0 \
        [lib/certdb]=0 \
        [lib/certhigh]=0 \
        [lib/ssl]=0 \
        [lib/freebl]=0 \
        [lib/util]=0 \
    )

# remove .OBJ directories to force a rebuild of just the select few
for i in "${!scan[@]}"; do
   find "$i" -name "*.OBJ" -exec rm -rf {} \+
done

# run scan-build (only building affected directories)
scan-build -o /home/worker/artifacts --use-cc=$CC --use-c++=$CCC make nss_build_all && cd ..

# print errors we found
set +v +x
STATUS=0
for i in "${!scan[@]}"; do
   n=$(grep -Rn "$i" /home/worker/artifacts/*/report-*.html | wc -l)
   if [ $n -ne ${scan[$i]} ]; then
     STATUS=1
     echo "$(date '+%T') WARNING - TEST-UNEXPECTED-FAIL: $i contains $n scan-build errors"
   elif [ $n -ne 0 ]; then
     echo "$(date '+%T') WARNING - TEST-EXPECTED-FAIL: $i contains $n scan-build errors"
   fi
done
exit $STATUS
