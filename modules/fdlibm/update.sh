#!/bin/sh

# Script to update the mozilla in-tree copy of the fdlibm library.
# Run this within the /modules/fdlibm directory of the source tree.

set -e

API_BASE_URL=https://api.github.com/repos/freebsd/freebsd-src

get_commit() {
    curl -s "${API_BASE_URL}/commits?path=lib/msun/src&per_page=1" \
        | python -c 'import json, sys; print(json.loads(sys.stdin.read())[0]["sha"])'
}
get_date() {
    curl -s "${API_BASE_URL}/commits/${COMMIT}" \
        | python -c 'import json, sys; print(json.loads(sys.stdin.read())["commit"]["committer"]["date"])'
}

mv ./src/moz.build ./src_moz.build
rm -rf src
if [ "$#" -eq 0 ]; then
    COMMIT=$(get_commit)
else
    COMMIT="$1"
fi
sh ./import.sh "${COMMIT}"
mv ./src_moz.build ./src/moz.build
COMMITDATE=$(get_date)
for FILE in $(ls patches/*.patch | sort); do
    echo "Applying ${FILE} ..."
    patch -p3 --no-backup-if-mismatch < ${FILE}
done
hg add src

perl -p -i -e "s/\[commit [0-9a-f]{40} \(.{1,100}\)\]/[commit ${COMMIT} (${COMMITDATE})]/" README.mozilla

echo "###"
echo "### Updated fdlibm/src to ${COMMIT}."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
