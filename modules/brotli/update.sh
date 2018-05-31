#!/bin/sh

# Script to update the mozilla in-tree copy of the Brotli decompressor.
# Run this within the /modules/brotli directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t brotli_update.XXXXXX` || exit 1

git clone https://github.com/google/brotli ${MY_TEMP_DIR}/brotli
git -C ${MY_TEMP_DIR}/brotli checkout v1.0.4

COMMIT=$(git -C ${MY_TEMP_DIR}/brotli rev-parse HEAD)
perl -p -i -e "s/\[commit [0-9a-f]{40}\]/[commit ${COMMIT}]/" README.mozilla;

DIRS="common dec enc include tools"

for d in $DIRS; do
	rm -rf $d
	mv ${MY_TEMP_DIR}/brotli/c/$d $d
done
rm -rf ${MY_TEMP_DIR}

hg addremove $DIRS

echo "###"
echo "### Updated brotli/dec to $COMMIT."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
