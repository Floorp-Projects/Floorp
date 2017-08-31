#!/usr/bin/env bash

if [[ $(id -u) -eq 0 ]]; then
    # Drop privileges by re-running this script.
    # Note: this mangles arguments, better to avoid running scripts as root.
    exec su worker -c "$0 $*"
fi

set -e -x -v

# The docker image this is running in has the HACL* and NSS sources.
# The extracted C code from HACL* is already generated and the HACL* tests were
# successfully executed.

# Format the extracted C code.
cd ~/hacl-star/snapshots/nss-production
cp ~/nss/.clang-format .
find . -type f -name '*.[ch]' -exec clang-format -i {} \+

# These diff commands will return 1 if there are differences and stop the script.
files=($(find ~/nss/lib/freebl/verified/ -type f -name '*.[ch]'))
for f in "${files[@]}"; do
    diff $f $(basename "$f")
done
