#!/bin/sh

# Script to update the mozilla in-tree copy of the libprio library.
# Run this within the /third_party/libprio directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t libprio_update.XXXXXX` || exit 1

COMMIT="a95cfdd5eaf7104582709c54ef23395d24d7f7fd"

git clone -n https://github.com/mozilla/libprio ${MY_TEMP_DIR}/libprio
git -C ${MY_TEMP_DIR}/libprio checkout ${COMMIT}

FILES="include prio"
VERSION=$(git -C ${MY_TEMP_DIR}/libprio describe --tags)
perl -p -i -e "s/Current version: \S+ \[commit [0-9a-f]{40}\]/Current version: ${VERSION} [commit ${COMMIT}]/" README-mozilla

for f in $FILES; do
    rm -rf $f
    mv ${MY_TEMP_DIR}/libprio/$f $f
done

rm -rf ${MY_TEMP_DIR}

hg revert -r . moz.build
hg addremove .

echo "###"
echo "### Updated libprio to $COMMIT."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
