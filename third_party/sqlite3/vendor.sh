#!/bin/bash

# IMPORTANT: use `./mach vendor third_party/sqlite3/moz.yaml`, don't invoke
#            this script directly.

# Script to download updated versions of SQLite sources and extensions.

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

# Retrieve SQLite information from the website script-friendly section.

echo ""
echo "Retrieving SQLite latest version..."
download_url="https://www.sqlite.org/download.html"
# The download page contains a script-friendly comment to extract file versions.
# PRODUCT,VERSION,RELATIVE-URL,SIZE-IN-BYTES,SHA3-HASH
# Match on the amalgamation to extract path and version, they are the same for
# all the files anyway.
re="PRODUCT,([^,]+),([^,]+)/sqlite-amalgamation-([0-9]+)\.zip"
DOWNLOAD_PAGE_HTML="`wget -t 3 --retry-connrefused -w 5 --random-wait $download_url -qO-`"
if [[ $DOWNLOAD_PAGE_HTML =~ $re ]]; then
  webversion="${BASH_REMATCH[1]}";
  path="${BASH_REMATCH[2]}";
  version="${BASH_REMATCH[3]}";
else
  echo "Error retrieving SQLite files";
  exit;
fi

# Check version matches with the one from Github, otherwise you'll have to point
# ./mach vendor to a specific revision.

echo ""
echo "Comparing Github and Website version numbers..."
gitversion=`cat src/VERSION.txt`
echo "Website version: $webversion";
echo "Github version: $gitversion";
if [ "$webversion" != "$gitversion" ]; then
  echo 'Versions do not match, try to invoke `./mach vendor` with a specific `--revision <github tag>`)'
  exit;
fi

# Retrieve files and update sources.

echo ""
echo "Retrieving SQLite amalgamation..."
amalgamation_url="https://www.sqlite.org/$path/sqlite-amalgamation-$version.zip"
wget -t 3 --retry-connrefused -w 5 --random-wait $amalgamation_url -qO amalgamation.zip
echo "Unpacking SQLite source files..."
unzip -p "amalgamation.zip" "sqlite-amalgamation-$version/sqlite3.c" > "src/sqlite3.c"
unzip -p "amalgamation.zip" "sqlite-amalgamation-$version/sqlite3.h" > "src/sqlite3.h"
mkdir -p ext
unzip -p "amalgamation.zip" "sqlite-amalgamation-$version/sqlite3ext.h" > "ext/sqlite3ext.h"
rm -f "amalgamation.zip"

echo ""
echo "Retrieving SQLite preprocessed..."
preprocessed_url="https://www.sqlite.org/$path/sqlite-preprocessed-$version.zip"
wget -t 3 --retry-connrefused -w 5 --random-wait $preprocessed_url -qO preprocessed.zip
echo "Unpacking FTS5 extension..."
unzip -p "preprocessed.zip" "sqlite-preprocessed-$version/fts5.c" > "ext/fts5.c"
rm -f "preprocessed.zip"

# Retrieve and update other SQLite extensions code.

# If the extension is hosted on Github or other supported platforms, you want
# to use `mach vendor` for it, rather than manually downloading it here.
# The same is valid for SQLite owned extensions that don't need preprocessing
# (e.g. carray.c/h). In general anything that is in sqlite-src archive is also
# in their official Github repo.

echo ""
echo "Update complete, please commit and check in your changes."
echo ""
