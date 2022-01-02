#!/bin/sh

# Script to update the mozilla in-tree copy of the msgpack library.
# Run this within the /third_party/msgpack directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t msgpack_update.XXXXXX` || exit 1

COMMIT="b6803a5fecbe321458faafd6a079dac466614ff9"

git clone -n https://github.com/msgpack/msgpack-c ${MY_TEMP_DIR}/msgpack
git -C ${MY_TEMP_DIR}/msgpack checkout ${COMMIT}

VERSION=$(git -C ${MY_TEMP_DIR}/msgpack describe --tags)
perl -p -i -e "s/Current version: \S+ \[commit [0-9a-f]{40}\]/Current version: ${VERSION} [commit ${COMMIT}]/" README-mozilla;
FILES="src include"

for f in $FILES; do
    rm -rf $f
    mv ${MY_TEMP_DIR}/msgpack/$f $f
done

rm -rf ${MY_TEMP_DIR}

hg revert -r . moz.build
hg addremove .

echo "###"
echo "### Updated msgpack to $COMMIT."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
