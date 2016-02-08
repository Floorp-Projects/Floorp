#!/bin/sh

# Script to update the mozilla in-tree copy of the woff2 library.
# Run this within the /modules/woff2 directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t woff2_update.XXXXXX` || exit 1

git clone https://github.com/google/woff2 ${MY_TEMP_DIR}/woff2

COMMIT=`(cd ${MY_TEMP_DIR}/woff2 && git log | head -n 1)`
perl -p -i -e "s/\[commit [0-9a-f]{40}\]/[${COMMIT}]/" README.mozilla;

rm -rf src
mv ${MY_TEMP_DIR}/woff2/src src
patch -p3 < redefine-unique_ptr.patch
rm -rf ${MY_TEMP_DIR}
hg add src

echo "###"
echo "### Updated woff2/src to $COMMIT."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
