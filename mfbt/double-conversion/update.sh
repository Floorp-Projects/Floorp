# Usage: ./update.sh <double-conversion-src-directory>
#
# Copies the needed files from a directory containing the original
# double-conversion source that we need.

# This was last updated with git rev e5b34421b763f7bf7e4f9081403db417d5a55a36.

set -e

cp $1/LICENSE ./
cp $1/README ./

# Includes
cp $1/src/*.h ./

# Source
cp $1/src/*.cc ./

patch -p3 < add-mfbt-api-markers.patch
patch -p3 < use-StandardInteger.patch
