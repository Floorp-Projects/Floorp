#!/usr/bin/env bash

if [[ $(id -u) -eq 0 ]]; then
    # Drop privileges by re-running this script.
    # Note: this mangles arguments, better to avoid running scripts as root.
    exec su worker -c "$0 $*"
fi

# Apply clang-format on the provided folder and verify that this doesn't change any file.
# If any file differs after formatting, the script eventually exits with 1.
# Any differences between formatted and unformatted files is printed to stdout to give a hint what's wrong.

# Includes a default set of directories NOT to clang-format on.
blacklist=(
     "./automation" \
     "./coreconf" \
     "./doc" \
     "./pkg" \
     "./tests" \
     "./lib/libpkix" \
     "./lib/zlib" \
     "./lib/sqlite" \
     "./gtests/google_test" \
     "./.hg" \
     "./out" \
)

top="$(dirname $0)/../.."
cd "$top"

if [ $# -gt 0 ]; then
    dirs=("$@")
else
    dirs=($(find . -maxdepth 2 -mindepth 1 -type d ! -path . \( ! -regex '.*/' \)))
fi

format_folder()
{
    for black in "${blacklist[@]}"; do
        if [[ "$1" == "$black"* ]]; then
            echo "skip $1"
            return 1
        fi
    done
    return 0
}

for dir in "${dirs[@]}"; do
    if format_folder "$dir" ; then
        c="${dir//[^\/]}"
        echo "formatting $dir ..."
        depth=""
        if [ "${#c}" == "1" ]; then
            depth="-maxdepth 1"
        fi
        find "$dir" $depth -type f \( -name '*.[ch]' -o -name '*.cc' \) -exec clang-format -i {} \+
    fi
done

TMPFILE=$(mktemp /tmp/$(basename $0).XXXXXX)
trap 'rm $TMPFILE' exit
if (cd $(dirname $0); hg root >/dev/null 2>&1); then
    hg diff --git "$top" | tee $TMPFILE
else
    git -C "$top" diff | tee $TMPFILE
fi
[[ ! -s $TMPFILE ]]
