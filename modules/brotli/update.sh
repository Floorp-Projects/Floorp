#!/bin/sh

# Script to update the mozilla in-tree copy of the Brotli decompressor.
# Run this within the /modules/brotli directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t brotli_update` || exit 1

git clone https://code.google.com/p/font-compression-reference/ ${MY_TEMP_DIR}

COMMIT=`(cd ${MY_TEMP_DIR} && git log | head -n 1)`
perl -p -i -e "s/\[commit [0-9a-f]{40}\]/[${COMMIT}]/" README.mozilla;

rm -rf dec
mv ${MY_TEMP_DIR}/brotli/dec dec
rm -rf ${MY_TEMP_DIR}
hg add dec

echo '###'
echo '### Updated brotli/dec to $COMMIT.'
echo '### Remember to verify and commit the changes to source control!'
echo '###'
