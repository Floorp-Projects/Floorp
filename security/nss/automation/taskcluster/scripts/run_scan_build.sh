#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Clone NSPR if needed.
if [ ! -d "nspr" ]; then
    hg_clone https://hg.mozilla.org/projects/nspr ./nspr default

    if [[ -f nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
      pushd nspr
      cat ../nss/nspr.patch | patch -p1
      popd
    fi
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
        [lib/ckfw]=0 \
        [lib/crmf]=0 \
        [lib/cryptohi]=0 \
        [lib/dev]=0 \
        [lib/freebl]=0 \
        [lib/nss]=0 \
        [lib/ssl]=0 \
        [lib/util]=0 \
    )

# remove .OBJ directories to force a rebuild of just the select few
for i in "${!scan[@]}"; do
   find "$i" -name "*.OBJ" -exec rm -rf {} \+
done

# run scan-build (only building affected directories)
scan-build-5.0 -o /home/worker/artifacts --use-cc=$CC --use-c++=$CCC make nss_build_all && cd ..

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
