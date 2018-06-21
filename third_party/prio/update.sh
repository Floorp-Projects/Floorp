#!/bin/sh

# Script to update the mozilla in-tree copy of the Prio library.
# Run this within the /third_party/prio directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t libprio_update.XXXXXX` || exit 1

VERSION=1.8.7

git clone https://github.com/mozilla/libprio ${MY_TEMP_DIR}/libprio
git -C ${MY_TEMP_DIR}/libprio checkout ${VERSION}

COMMIT=$(git -C ${MY_TEMP_DIR}/libprio rev-parse HEAD)
perl -p -i -e "s/(\d+\.)(\d+\.)(\d+)/${VERSION}/" README-mozilla;
perl -p -i -e "s/\[commit [0-9a-f]{40}\]/[commit ${COMMIT}]/" README-mozilla;

FILES="LICENSE README.md SConstruct browser-test include pclient prio ptest"

for f in $FILES; do
    rm -rf $f
    mv ${MY_TEMP_DIR}/libprio/$f $f
done

rm -rf ${MY_TEMP_DIR}

hg revert -r . moz.build
hg addremove

echo "###"
echo "### Updated Prio to $COMMIT."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
