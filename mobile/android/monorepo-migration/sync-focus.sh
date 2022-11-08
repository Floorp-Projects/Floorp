#!/usr/bin/env bash

set -ex

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
CURRENT_REPO_PATH="$(dirname -- "$SCRIPT_DIR")"

REPO_NAME_TO_SYNC='focus-android'
MAIN_BRANCH_NAME='main'
PREP_BRANCH_NAME='focus-prep'
TMP_REPO_PATH="/tmp/git/$REPO_NAME_TO_SYNC"
TMP_REPO_BRANCH_NAME='firefox-android'
MONOREPO_URL='git@github.com:mozilla-mobile/firefox-android.git'
MERGE_COMMIT_MESSAGE=$(cat <<EOF
Merge https://github.com/mozilla-mobile/$REPO_NAME_TO_SYNC repository

The history was slightly altered before merging it:
  * All files from $REPO_NAME_TO_SYNC are now under its own subdirectory
  * All commits messages were rewritten to link issues and pull requests to the former repository
  * All commits messages were prefixed with [focus]
EOF
)

EXPRESSIONS_FILE_PATH="$SCRIPT_DIR/data/message-expressions.txt"
UTC_NOW="$(date -u '+%Y%m%d%H%M%S')"


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
    git fetch origin "$TMP_REPO_BRANCH_NAME"
}

function _update_repo_branch() {
    git checkout "$TMP_REPO_BRANCH_NAME"
    git rebase main
    git push origin "$TMP_REPO_BRANCH_NAME" --force
}

function _update_repo_numbers() {
    cd "$CURRENT_REPO_PATH"
    "$SCRIPT_DIR/generate-repo-numbers.py"
    "$SCRIPT_DIR/generate-replace-message-expressions.py"
    git switch "$MAIN_BRANCH_NAME"
    git add 'monorepo-migration/data'
    git commit -m "monorepo-migration: Fetch latest repo numbers and regexes"
    git switch -
}

function _rewrite_git_history() {
    cd "$TMP_REPO_PATH"
    git filter-repo \
        --to-subdirectory-filter "$REPO_NAME_TO_SYNC/" \
        --replace-message "$EXPRESSIONS_FILE_PATH" \
        --force
}

function _back_up_prep_branch() {
    cd "$CURRENT_REPO_PATH"
    if git rev-parse --quiet --verify "$PREP_BRANCH_NAME" > /dev/null; then
        git branch --move "$PREP_BRANCH_NAME" "$PREP_BRANCH_NAME-backup-$UTC_NOW"
    fi
}

function _reset_prep_branch() {
    _back_up_prep_branch
    cd "$CURRENT_REPO_PATH"
    git checkout "$MAIN_BRANCH_NAME"
    git pull
    git checkout -b "$PREP_BRANCH_NAME"
}

function _merge_histories() {
    cd "$CURRENT_REPO_PATH"
    git pull --no-edit --allow-unrelated-histories --no-rebase --force "$TMP_REPO_PATH"
    git commit --amend --message "$MERGE_COMMIT_MESSAGE"
}


_test_prerequisites
_setup_temporary_repo
_update_repo_branch
_update_repo_numbers
_rewrite_git_history
_reset_prep_branch
_merge_histories


cat <<EOF
$REPO_NAME_TO_SYNC has been sync'd and merged to the current branch.
You can now inspect the changes and push them once ready:

git push
EOF
