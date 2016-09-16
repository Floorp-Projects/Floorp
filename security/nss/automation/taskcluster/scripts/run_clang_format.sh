#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) -eq 0 ]; then
    # Drop privileges by re-running this script.
    exec su worker $0 "$@"
fi

# Apply clang-format on the provided folder and verify that this doesn't change any file.
# If any file differs after formatting, the script eventually exits with 1.
# Any differences between formatted and unformatted files is printed to stdout to give a hint what's wrong.

# Includes a default set of directories.

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
         "$top/lib/nss" \
         "$top/lib/softoken" \
         "$top/lib/ssl" \
         "$top/lib/util" \
         "$top/lib/sysinit" \
         "$top/external_tests/common" \
         "$top/external_tests/der_gtest" \
         "$top/external_tests/pk11_gtest" \
         "$top/external_tests/ssl_gtest" \
         "$top/external_tests/util_gtest" \
    )
fi

for dir in "${dirs[@]}"; do
    find "$dir" -type f \( -name '*.[ch]' -o -name '*.cc' \) -exec clang-format -i {} \+
done

TMPFILE=$(mktemp /tmp/$(basename $0).XXXXXX)
trap 'rm $TMPFILE' exit
if (cd $(dirname $0); hg root >/dev/null 2>&1); then
    hg diff --git "$top" | tee $TMPFILE
else
    git -C "$top" diff | tee $TMPFILE
fi
[[ ! -s $TMPFILE ]]
