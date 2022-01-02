#!/bin/sh
  
# Script to update the mozilla in-tree copy of CUPS headers
# Run this within the /third_party/cups directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t cups_update.XXXXXX` || exit 1

COMMIT='82e3ee0e3230287b76a76fb8f16b92ca6e50b444'
FILES='cups/cups.h cups/array.h cups/file.h cups/http.h cups/ipp.h cups/language.h cups/pwg.h cups/versioning.h'
git clone git://github.com/apple/cups.git "$MY_TEMP_DIR" --depth 1
git -C "$MY_TEMP_DIR" fetch origin "$COMMIT"
git -C "$MY_TEMP_DIR" reset --hard "$COMMIT"

mkdir -p include/cups 2> /dev/null
rm -rf include/cups/*

for FILE in $FILES ; do
    mv "$MY_TEMP_DIR/$FILE" "include/$FILE"
done

rm -rf "$MY_TEMP_DIR"

echo "###"
echo "### Updated cups to $COMMIT."
echo "### Remember to verify and commit the changes to source control!"
echo "###"

