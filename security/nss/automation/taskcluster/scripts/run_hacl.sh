#!/usr/bin/env bash

if [[ $(id -u) -eq 0 ]]; then
    # Drop privileges by re-running this script.
    # Note: this mangles arguments, better to avoid running scripts as root.
    exec su worker -c "$0 $*"
fi

set -e -x -v

# The docker image this is running in has NSS sources.
# Get the HACL* source, containing a snapshot of the C code, extracted on the
# HACL CI.
git clone -q "https://github.com/hacl-star/hacl-star" ~/hacl-star
git -C ~/hacl-star checkout -q 5f6051d2134cda490c890e57bb16da0110744646

# Format the C snapshot.
cd ~/hacl-star/dist/mozilla
cp ~/nss/.clang-format .
find . -type f -name '*.[ch]' -exec clang-format -i {} \+
cd ~/hacl-star/dist/karamel
cp ~/nss/.clang-format .
find . -type f -name '*.[ch]' -exec clang-format -i {} \+

# These diff commands will return 1 if there are differences and stop the script.

# We have two checks in the script. 
# The first one only checks the files in the verified/internal folder; the second one does for all the rest
# It was implemented like this due to not uniqueness of the names in the verified folders
# For instance, the files Hacl_Chacha20.h are present in both directories, but the content differs.

files=($(find ~/nss/lib/freebl/verified/internal -type f -name '*.[ch]'))
for f in "${files[@]}"; do
    file_name=$(basename "$f")
    hacl_file=($(find ~/hacl-star/dist/mozilla/internal/ -type f -name $file_name))
    diff $hacl_file $f
done

files=($(find ~/nss/lib/freebl/verified/ -type f -name '*.[ch]' -not -path "*/freebl/verified/internal/*" -not -path "*/freebl/verified/config.h"))
for f in "${files[@]}"; do
    file_name=$(basename "$f")
    hacl_file=($(find ~/hacl-star/dist/mozilla/ ~/hacl-star/dist/karamel/ -type f -name $file_name -not -path "*/hacl-star/dist/mozilla/internal/*"))
    diff $hacl_file $f
done
