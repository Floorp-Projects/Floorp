#!/usr/bin/env bash

set -ex

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
CURRENT_REPO_PATH="$(dirname -- "$SCRIPT_DIR")"

REPO_NAME_TO_SYNC='android-components'
TMP_REPO_PATH="/tmp/git/$REPO_NAME_TO_SYNC"
REPO_BRANCH_NAME='firefox-android'
TAG_PREFIX='components-'
MONOREPO_URL='git@github.com:mozilla-mobile/firefox-android.git'
MERGE_COMMIT_MESSAGE=$(cat <<EOF
Merge https://github.com/mozilla-mobile/$REPO_NAME_TO_SYNC repository

The history was slightly altered before merging it:
  * All files from $REPO_NAME_TO_SYNC are now under its own subdirectory
  * All commits messages were rewritten to link issues and pull requests to the former repository
  * All commits messages were prefixed with [components]
  * All tags were were prefixed with $TAG_PREFIX
EOF
)

EXPRESSIONS_FILE_PATH="$SCRIPT_DIR/data/message-expressions.txt"


function _is_github_authenticated() {
    set +e
    ssh -T git@github.com
    exit_code=$?
    set -e
    if [[ $exit_code == 1 ]]; then
        # user is authenticated, but fails to open a shell with GitHub
        return 0
    fi
    exit "$exit_code"
}

function _test_prerequisites() {
    _is_github_authenticated
    git filter-repo --version > /dev/null || (echo 'ERROR: Please install git-filter-repo: https://github.com/newren/git-filter-repo/blob/main/INSTALL.md'; exit 1)
}

function _setup_temporary_repo() {
    rm -rf "$TMP_REPO_PATH"
    mkdir -p "$TMP_REPO_PATH"

    git clone "git@github.com:mozilla-mobile/$REPO_NAME_TO_SYNC.git" "$TMP_REPO_PATH"
    cd "$TMP_REPO_PATH"
    git fetch origin "$REPO_BRANCH_NAME"
}

function _update_repo_branch() {
    git checkout "$REPO_BRANCH_NAME"
    git rebase main
    git push origin "$REPO_BRANCH_NAME" --force
}

function _update_repo_numbers() {
    "$SCRIPT_DIR/generate-repo-numbers.py"
    "$SCRIPT_DIR/generate-replace-message-expressions.py"
}

function _rewrite_git_history() {
    git filter-repo \
        --to-subdirectory-filter "$REPO_NAME_TO_SYNC/" \
        --replace-message "$EXPRESSIONS_FILE_PATH" \
        --tag-rename '':"$TAG_PREFIX" \
        --force
}

function _remove_old_tags() {
    cd "$SCRIPT_DIR"
    set +e
    git tag | grep "$TAG_PREFIX" | xargs -L 1 | xargs git push "$MONOREPO_URL" --delete
    set -e
}

function _merge_histories() {
    cd "$SCRIPT_DIR"
    git pull --no-edit --tags --allow-unrelated-histories --no-rebase --force "$TMP_REPO_PATH"
    git commit --amend --message "$MERGE_COMMIT_MESSAGE"
}

function _move_files_into_subfolder() {
    cd "$CURRENT_REPO_PATH"
    git mv --force 'l10n.toml' "$REPO_NAME_TO_SYNC/l10n.toml"
    git commit --message 'Move l10n.toml into android-components subfolder'
}

function _clean_up_temporary_repo() {
    rm -rf "$TMP_REPO_PATH"
}


_test_prerequisites
_setup_temporary_repo
_update_repo_branch
_update_repo_numbers
_rewrite_git_history
_remove_old_tags
_merge_histories
_move_files_into_subfolder
_clean_up_temporary_repo


cat <<EOF
$REPO_NAME_TO_SYNC has been sync'd and merged to the current branch.
You can now inspect the changes and push them once ready:

git push
git push --tags
EOF
