#!/bin/sh
self="$0"
dirname_self=$(dirname "$self")

usage() {
  cat <<EOF >&2
Usage: $self [option]

This script applies a whitespace transformation to the commit at HEAD. If no
options are given, then the modified files are left in the working tree.

Options:
  -h, --help     Shows this message
  -n, --dry-run  Shows a diff of the changes to be made.
  --amend        Squashes the changes into the commit at HEAD
                     This option will also reformat the commit message.
  --commit       Creates a new commit containing only the whitespace changes
  --msg-only     Reformat the commit message only, ignore the patch itself.

EOF
  rm -f ${CLEAN_FILES}
  exit 1
}


log() {
  echo "${self##*/}: $@" >&2
}


aom_style() {
  for f; do
    case "$f" in
      *.h|*.c|*.cc)
        clang-format -i --style=file "$f"
        ;;
    esac
  done
}


apply() {
  [ $INTERSECT_RESULT -ne 0 ] && patch -p1 < "$1"
}


commit() {
  LAST_CHANGEID=$(git show | awk '/Change-Id:/{print $2}')
  if [ -z "$LAST_CHANGEID" ]; then
    log "HEAD doesn't have a Change-Id, unable to generate a new commit"
    exit 1
  fi

  # Build a deterministic Change-Id from the parent's
  NEW_CHANGEID=${LAST_CHANGEID}-styled
  NEW_CHANGEID=I$(echo $NEW_CHANGEID | git hash-object --stdin)

  # Commit, preserving authorship from the parent commit.
  git commit -a -C HEAD > /dev/null
  git commit --amend -F- << EOF
Cosmetic: Fix whitespace in change ${LAST_CHANGEID:0:9}

Change-Id: ${NEW_CHANGEID}
EOF
}


show_commit_msg_diff() {
  if [ $DIFF_MSG_RESULT -ne 0 ]; then
    log "Modified commit message:"
    diff -u "$ORIG_COMMIT_MSG" "$NEW_COMMIT_MSG" | tail -n +3
  fi
}


amend() {
  show_commit_msg_diff
  if [ $DIFF_MSG_RESULT -ne 0 ] || [ $INTERSECT_RESULT -ne 0 ]; then
    git commit -a --amend -F "$NEW_COMMIT_MSG"
  fi
}


diff_msg() {
  git log -1 --format=%B > "$ORIG_COMMIT_MSG"
  "${dirname_self}"/wrap-commit-msg.py \
      < "$ORIG_COMMIT_MSG" > "$NEW_COMMIT_MSG"
  cmp -s "$ORIG_COMMIT_MSG" "$NEW_COMMIT_MSG"
  DIFF_MSG_RESULT=$?
}


# Temporary files
ORIG_DIFF=orig.diff.$$
MODIFIED_DIFF=modified.diff.$$
FINAL_DIFF=final.diff.$$
ORIG_COMMIT_MSG=orig.commit-msg.$$
NEW_COMMIT_MSG=new.commit-msg.$$
CLEAN_FILES="${ORIG_DIFF} ${MODIFIED_DIFF} ${FINAL_DIFF}"
CLEAN_FILES="${CLEAN_FILES} ${ORIG_COMMIT_MSG} ${NEW_COMMIT_MSG}"

# Preconditions
[ $# -lt 2 ] || usage

if ! clang-format -version >/dev/null 2>&1; then
  log "clang-format not found"
  exit 1
fi

if ! git diff --quiet HEAD; then
  log "Working tree is dirty, commit your changes first"
  exit 1
fi

# Need to be in the root
cd "$(git rev-parse --show-toplevel)"

# Collect the original diff
git show > "${ORIG_DIFF}"

# Apply the style guide on new and modified files and collect its diff
for f in $(git diff HEAD^ --name-only -M90 --diff-filter=AM); do
  case "$f" in
    third_party/*) continue;;
  esac
  aom_style "$f"
done
git diff --no-color --no-ext-diff > "${MODIFIED_DIFF}"

# Intersect the two diffs
"${dirname_self}"/intersect-diffs.py \
    "${ORIG_DIFF}" "${MODIFIED_DIFF}" > "${FINAL_DIFF}"
INTERSECT_RESULT=$?
git reset --hard >/dev/null

# Fixup the commit message
diff_msg

# Handle options
if [ -n "$1" ]; then
  case "$1" in
    -h|--help) usage;;
    -n|--dry-run) cat "${FINAL_DIFF}"; show_commit_msg_diff;;
    --commit) apply "${FINAL_DIFF}"; commit;;
    --amend) apply "${FINAL_DIFF}"; amend;;
    --msg-only) amend;;
    *) usage;;
  esac
else
  apply "${FINAL_DIFF}"
  if ! git diff --quiet; then
    log "Formatting changes applied, verify and commit."
    log "See also: http://www.webmproject.org/code/contribute/conventions/"
    git diff --stat
  fi
fi

rm -f ${CLEAN_FILES}
