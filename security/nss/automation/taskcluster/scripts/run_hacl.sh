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

# Verify HACL*. Taskcluster fails when we do this in the image build.
make -C hacl-star verify-nss -j$(nproc)

# Add license header to specs
spec_files=($(find ~/hacl-star/specs -type f -name '*.fst'))
for f in "${spec_files[@]}"; do
    cat /tmp/license.txt "$f" > /tmp/tmpfile && mv /tmp/tmpfile "$f"
done

# Format the extracted C code.
cd ~/hacl-star/snapshots/nss
cp ~/nss/.clang-format .
find . -type f -name '*.[ch]' -exec clang-format -i {} \+

# These diff commands will return 1 if there are differences and stop the script.
files=($(find ~/nss/lib/freebl/verified/ -type f -name '*.[ch]'))
for f in "${files[@]}"; do
    diff $f $(basename "$f")
done

# Check that the specs didn't change either.
cd ~/hacl-star/specs
files=($(find ~/nss/lib/freebl/verified/specs -type f))
for f in "${files[@]}"; do
    diff $f $(basename "$f")
done
