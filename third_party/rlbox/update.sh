#!/bin/sh

# Script to update the mozilla in-tree copy of the rlbox library.
# Run this within the /third_party/rlbox directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t rlbox_update.XXXXXX` || exit 1

git clone https://github.com/PLSysSec/rlbox_sandboxing_api ${MY_TEMP_DIR}/rlbox

COMMIT=$(git -C ${MY_TEMP_DIR}/rlbox rev-parse HEAD)
perl -p -i -e "s/\[commit [0-9a-f]{40}\]/[commit ${COMMIT}]/" README-mozilla;

FILES="include"

for f in $FILES; do
    rm -rf $f
    mv ${MY_TEMP_DIR}/rlbox/code/$f $f
done

rm -rf ${MY_TEMP_DIR}

hg addremove $FILES

echo "###"
echo "### Updated rlbox to $COMMIT."
echo "### Remember to update any newly added files to /config/external/rlbox/moz.build"
echo "### Remember to verify and commit the changes to source control!"
echo "###"
