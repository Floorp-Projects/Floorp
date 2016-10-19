#!/bin/sh

# Script to update the Mozilla in-tree copy of XZ Embedded.

MY_TEMP_DIR=$(mktemp -d -t xz-embedded_update.XXXXXX) || exit 1

git clone http://git.tukaani.org/xz-embedded.git ${MY_TEMP_DIR}/xz-embedded

COMMIT=$(git -C ${MY_TEMP_DIR}/xz-embedded rev-parse HEAD)
cd $(dirname $0)
perl -p -i -e "s/\[commit [0-9a-f]{40}\]/[${COMMIT}]/" README.mozilla;

rm -rf src
mkdir src
mv ${MY_TEMP_DIR}/xz-embedded/userspace/xz_config.h src/
mv ${MY_TEMP_DIR}/xz-embedded/linux/include/linux/xz.h src/
mv ${MY_TEMP_DIR}/xz-embedded/linux/lib/xz/xz_private.h src/
mv ${MY_TEMP_DIR}/xz-embedded/linux/lib/xz/xz_lzma2.h src/
mv ${MY_TEMP_DIR}/xz-embedded/linux/lib/xz/xz_stream.h src/
mv ${MY_TEMP_DIR}/xz-embedded/linux/lib/xz/xz_crc32.c src/
mv ${MY_TEMP_DIR}/xz-embedded/linux/lib/xz/xz_crc64.c src/
mv ${MY_TEMP_DIR}/xz-embedded/linux/lib/xz/xz_dec_bcj.c src/
mv ${MY_TEMP_DIR}/xz-embedded/linux/lib/xz/xz_dec_stream.c src/
mv ${MY_TEMP_DIR}/xz-embedded/linux/lib/xz/xz_dec_lzma2.c src/
rm -rf ${MY_TEMP_DIR}
hg addremove src

echo "###"
echo "### Updated xz-embedded/src to $COMMIT."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
