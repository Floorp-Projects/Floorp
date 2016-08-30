#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) -eq 0 ]; then
    # Drop privileges by re-running this script.
    exec su worker $0 "$@"
fi

# Apply clang-format 3.8 on the provided folder and verify that this doesn't change any file.
# If any file differs after formatting, the script eventually exits with 1.
# Any differences between formatted and unformatted files is printed to stdout to give a hint what's wrong.

# Includes a default set of directories.

apply=false
if [ $1 = "--apply" ]; then
    apply=true
    shift
fi

if [ $# -gt 0 ]; then
    dirs=("$@")
else
    top=$(dirname $0)/../../..
    dirs=( \
         "$top/cmd" \
         "$top/lib/base" \
         "$top/lib/certdb" \
         "$top/lib/certhigh" \
         "$top/lib/ckfw" \
         "$top/lib/crmf" \
         "$top/lib/cryptohi" \
         "$top/lib/dbm" \
         "$top/lib/dev" \
         "$top/lib/freebl" \
         "$top/lib/softoken" \
         "$top/lib/ssl" \
         "$top/external_tests/common" \
         "$top/external_tests/der_gtest" \
         "$top/external_tests/pk11_gtest" \
         "$top/external_tests/ssl_gtest" \
         "$top/external_tests/util_gtest" \
    )
fi

STATUS=0
for dir in "${dirs[@]}"; do
    for i in $(find "$dir" -type f \( -name '*.[ch]' -o -name '*.cc' \) -print); do
        if $apply; then
            clang-format -i "$i"
        elif ! clang-format "$i" | diff -Naur "$i" -; then
            echo "Sorry, $i is not formatted properly. Please use clang-format 3.8 on your patch before landing."
            STATUS=1
        fi
    done
done
exit $STATUS
