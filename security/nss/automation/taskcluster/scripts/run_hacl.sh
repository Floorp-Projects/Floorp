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
# When bug 1593647 is resolved, extract the code on CI again.
git clone -q "https://github.com/project-everest/hacl-star" ~/hacl-star
git -C ~/hacl-star checkout -q 079854e0072041d60859b6d8af2743bc6a37dc05

# Format the C snapshot.
cd ~/hacl-star/dist/mozilla
cp ~/nss/.clang-format .
find . -type f -name '*.[ch]' -exec clang-format -i {} \+
cd ~/hacl-star/dist/kremlin
cp ~/nss/.clang-format .
find . -type f -name '*.[ch]' -exec clang-format -i {} \+

# These diff commands will return 1 if there are differences and stop the script.
files=($(find ~/nss/lib/freebl/verified/ -type f -name '*.[ch]'))
for f in "${files[@]}"; do
    file_name=$(basename "$f")
    hacl_file=($(find ~/hacl-star/dist/mozilla/ ~/hacl-star/dist/kremlin/ -type f -name $file_name))
    diff $hacl_file $f
done
