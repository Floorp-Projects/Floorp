#!/bin/sh

set -e
set -x

# FIXME: make rename detection configurable and switch to glandium/git-cinnabar:master
CINNABAR=https://github.com/jcristau/git-cinnabar
CINNABAR_BRANCH=detect-more-renames

SOURCE_REPO=https://github.com/mozilla-mobile/firefox-android
SOURCE_REPO_NAME=${SOURCE_REPO##*/}
SOURCE_BRANCH=main

DEST_REPO="$(readlink -f "$1")"
DEST_SUBDIR=mobile/android

EXPRESSIONS_FILE_PATH="$(dirname $(readlink -f "$0"))/data/message-expressions.txt"

TEMPDIR=$(mktemp -d)
trap "rm -rf ${TEMPDIR}" EXIT
CINNABAR_PATH="${TEMPDIR}/git-cinnabar"
SOURCE_REPO_PATH="${TEMPDIR}/${SOURCE_REPO_NAME}"

setup_temporary_repo() {
  git clone "${SOURCE_REPO}" "${SOURCE_REPO_PATH}"
  git -C "${SOURCE_REPO_PATH}" config user.email=release@mozilla.com
  git -C "${SOURCe_REPO_PATH}" config user.name="Mozilla Release Engineering"
}

setup_cinnabar() {
  git clone --branch "${CINNABAR_BRANCH}" --depth 1 "${CINNABAR}" "${CINNABAR_PATH}"
  make -C "${CINNABAR_PATH}"
}

rewrite_git_history() {
  # replace octopus merges with a series of two-parent merges
  # commit-filter is called with args "<tree> [-p <parent>]*" and with the commit message on stdin
  FILTER_BRANCH_SQUELCH_WARNING=1 \
  git -C "${SOURCE_REPO_PATH}" filter-branch \
    --force \
    --commit-filter \
      'if test "$#" -lt 7; then
         # 0, 1 or 2 parents, call commit-tree as-is
         git commit-tree "$@"
       else
         # octopus merge, replace it with a series of manual merges
         commit_message=$(mktemp)
         cat > $commit_message

         final_tree="$1"
         shift
         shift
         p1="$1"
         shift
         part=1
         while test "$#" -ge 2; do
           shift
           p2="$1"
           shift
           tree=$(git merge-tree --write-tree --allow-unrelated-histories "$p1" "$p2")
           p1="$(sed "1s/$/ (part $part)/" $commit_message | git commit-tree "$tree" -p "$p1" -p "$p2")"
           part=$((part + 1))
         done
         rm -f $commit_message
         # sanity check that the end result is what we expected
         git diff-tree --exit-code $tree $final_tree
         echo "$p1"
     fi' \
        -- "$SOURCE_BRANCH"
  # to-subdirectory-filter: move everything to $DEST_SUBDIR
  # replace-message: replace references to pull requests with the full URL
  # preserve-commit-hashes: because the filter-branch step also changes
  #                         hashes, as does the conversion to hg, rewriting
  #                         hashes here would only point at temporary commit
  #                         objects which wouldn't help
  git -C "${SOURCE_REPO_PATH}" filter-repo \
    --force \
    --to-subdirectory-filter "$DEST_SUBDIR/" \
    --replace-message "$EXPRESSIONS_FILE_PATH" \
    --preserve-commit-hashes \
    --refs "$SOURCE_BRANCH"
}

convert_to_hg() {
  PATH="${PATH}:${CINNABAR_PATH}" \
  git -C "${SOURCE_REPO_PATH}" \
      -c cinnabar.experiments=merge \
      push "hg::${DEST_REPO}" "${SOURCE_BRANCH}"
}

setup_cinnabar
setup_temporary_repo
rewrite_git_history
if ! [ -d "${DEST_REPO}" ]; then
  hg init "${DEST_REPO}"
fi
convert_to_hg
