#!/bin/sh

# Script to update the mozilla in-tree copy of the fdlibm library.
# Run this within the /modules/fdlibm directory of the source tree.

set -e

API_BASE_URL=https://api.github.com/repos/freebsd/freebsd

get_commit() {
    curl -s "${API_BASE_URL}/commits?path=lib/msun/src&per_page=1" \
        | python -c 'import json, sys; print(json.loads(sys.stdin.read())[0]["sha"])'
}

mv ./src/moz.build ./src_moz.build
rm -rf src
BEFORE_COMMIT=$(get_commit)
sh ./import.sh
mv ./src_moz.build ./src/moz.build
COMMIT=$(get_commit)
if [ ${BEFORE_COMMIT} != ${COMMIT} ]; then
    echo "Latest commit is changed during import.  Please run again."
    exit 1
fi
for FILE in $(ls patches/*.patch | sort); do
    patch -p3 < ${FILE}
done
hg add src

perl -p -i -e "s/\[commit [0-9a-f]{40}\]/[commit ${COMMIT}]/" README.mozilla

echo "###"
echo "### Updated fdlibm/src to ${COMMIT}."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
