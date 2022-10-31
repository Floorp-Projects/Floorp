#!/usr/bin/env python3

# Purpose: uplift (via cherry-picking) any missing commits from an l10n bot
# from 'MAIN_BRANCH' to a specified release branch.
#
# Usage examples: (append --verbose to print out detailed information)
# Dry-run (says what will happen, doesn't do any work): ./l10n-uplift.py releases/48.0
# Uplift, actually perform the work: ./l10n-uplift.py releases/48.0 --uplift
# Process multiple branches at once: ./l10n-uplift.py releases/48.0 releases/44.0 --uplift --verbose

import subprocess
import argparse

# TODO don't forget to change this once we switch to 'main' or whatever other name.
MAIN_BRANCH="master"
L10N_AUTHOR="release+l10n-automation-bot@mozilla.com"

def run_cmd_checked(*args, **kwargs):
    """Run a command, throwing an exception if it exits with non-zero status."""
    kwargs["check"] = True
    kwargs["capture_output"] = True
    # beware! only run this script with inputs from a trusted, non-external source
    kwargs["shell"] = True
    try:
        return subprocess.run(*args, **kwargs).stdout.decode()
    except subprocess.CalledProcessError as err:
        print(err.stderr)
        raise err

def uplift_commits(branch, verbose, uplift):
    print(f"\nProcessing l10n commits for '{branch}'...")
    # if necessary, this will setup 'branch' to track its upstream equivalent
    run_cmd_checked([f"git checkout {branch}"])
    # get l10n commits which happened on MAIN_BRANCH since 'branch' split off
    commits_since_split = run_cmd_checked([f"git rev-list {branch}..{MAIN_BRANCH} --author={L10N_AUTHOR}"]).split()
    # order commits by oldest-first, e.g. how we'd cherry pick them
    commits_since_split.reverse()
    print(f"Since '{branch}' split off '{MAIN_BRANCH}', there were {len(commits_since_split)} commit(s) from {L10N_AUTHOR}.")

    if verbose:
        print(f"\nHashes of those commits on '{MAIN_BRANCH}' are: {commits_since_split}\n")

    # look for 'cherry picked' commits, and get the original commit hash from the commit message (as left by 'cherry-pick -x')
    commits_already_uplifted = run_cmd_checked([f"git rev-list {MAIN_BRANCH}..{branch} --author={L10N_AUTHOR} --grep=\"cherry picked\" --pretty=%b | grep cherry | cut -d' ' -f5 | cut -c 1-40"]).split()
    commits_already_uplifted.reverse()

    print(f"Of those, {len(commits_already_uplifted)} commit(s) already uplifted.")

    if verbose:
        print(f"Hashes of commits already uplifted to '{branch}': {commits_already_uplifted}\n")

    commits_to_uplift = [commit for commit in commits_since_split if commit not in commits_already_uplifted]

    print(f"Need to uplift {len(commits_to_uplift)} commit(s).")

    if verbose:
        print(f"Hashes of commits to uplift from '{MAIN_BRANCH}' to '{branch}': {commits_to_uplift}\n")

    if len(commits_to_uplift) == 0:
        print("Nothing to uplift.")
        return

    if uplift:
        print(f"Uplifting (for real)...")
    else:
        print(f"Uplifting (dry-run)...")

    run_cmd_checked([f"git checkout {branch}"])
    for commit in commits_to_uplift:
        if verbose:
            print(f"Cherry picking {commit} from '{MAIN_BRANCH}' to '{branch}'")
        if uplift:
            run_cmd_checked([f"git cherry-pick {commit} -x"])
    if uplift:
        print(f"Uplifted {len(commits_to_uplift)} commits from '{MAIN_BRANCH}' to '{branch}'")

parser = argparse.ArgumentParser(description=f"Uplift l10n commits from {MAIN_BRANCH} to specified branches")
parser.add_argument(
    'branches', nargs='+', type=str,
    help='target branches, e.g. specific release branches')
parser.add_argument(
    '--verbose', default=False, action='store_true',
    help='print out commit hashes and other detailed information'
)
parser.add_argument(
    '--uplift', default=False, action='store_true',
    help='uplift l10n commits missing from specified branches (if not specified, dry-run is performed)'
)
args = parser.parse_args()

# remember the current branch, so that we can return to it once we're done.
current_branch = run_cmd_checked(["git rev-parse --abbrev-ref HEAD"])

try:
    for branch in args.branches:
        uplift_commits(branch, args.verbose, args.uplift)
finally:
    # go back to the branch we were on before 'uplift_for_branches' ran
    run_cmd_checked([f"git checkout {current_branch}"])
