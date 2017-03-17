#!/usr/bin/env bash
# This is a pre-commit hook for use with either mercurial or git.
#
# Install this by running the script with an argument of "install".
#
# All that does is add the following lines to .hg/hgrc:
#
# [hook]
# pretxncommit.clang-format = [ ! -x ./coreconf/precommit.clang-format.sh ] || ./coreconf/precommit.clang-format.sh
#
# Or installs a symlink to .git/hooks/precommit:
# $ ln -s ../../coreconf/precommit.clang-format.sh .git/hooks/pre-commit

hash clang-format || exit 1
[ "$(hg root 2>/dev/null)" = "$PWD" ] && hg=1 || hg=0
[ "$(git rev-parse --show-toplevel 2>/dev/null)" = "$PWD" ] && git=1 || git=0

if [ "$1" = "install" ]; then
    if [ "$hg" -eq 1 ]; then
        hgrc="$(hg root)"/.hg/hgrc
        if ! grep -q '^pretxncommit.clang-format' "$hgrc"; then
            echo '[hooks]' >> "$hgrc"
            echo 'pretxncommit.clang-format = [ ! -x ./coreconf/precommit.clang-format.sh ] || ./coreconf/precommit.clang-format.sh' >> "$hgrc"
            echo "Installed mercurial pretxncommit hook"
            exit
        fi
    fi
    if [ "$git" -eq 1 ]; then
        hook="$(git rev-parse --show-toplevel)"/.git/hooks/pre-commit
        if [ ! -e "$hook" ]; then
            ln -s ../../coreconf/precommit.clang-format.sh "$hook"
            echo "Installed git pre-commit hook"
            exit
        fi
    fi
    echo "Hook already installed, or not in NSS repo"
    exit 2
fi

err=0
files=()
if [ "$hg" -eq 1 ]; then
    files=($(hg status -m -a --rev tip^:tip | cut -f 2 -d ' ' -))
fi
if [ "$git" -eq 1 ]; then
    files=($(git status --porcelain | sed '/^[MACU]/{s/..//;p;};/^R/{s/^.* -> //;p;};d'))
fi
tmp=$(mktemp)
trap 'rm -f "$tmp"' ERR EXIT
for f in "${files[@]}"; do
    ext="${f##*.}"
    if [ "$ext" = "c" -o "$ext" = "h" -o "$ext" = "cc" ]; then
        [ "$hg" -eq 1 ] && hg cat -r tip "$f" > "$tmp"
        [ "$git" -eq 1 ] && git show :"$f" > "$tmp"
        if ! cat "$tmp" | clang-format -assume-filename="$f" | \
            diff -q "$tmp" - >/dev/null; then
            [ "$err" -eq 0 ] && echo "Formatting errors found in:" 1>&2
            echo "  $f" 1>&2
            err=1
        fi
    fi
done
exit "$err"
