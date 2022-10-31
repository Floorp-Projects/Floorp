#!/usr/bin/env python3

# Purpose: Publish android packages to local maven repo, but only if changed since last publish.
# Dependencies: None
# Usage: ./automation/publish_to_maven_local_if_modified.py

from pathlib import Path
import sys
import os
import time
import hashlib
import argparse
import re
import subprocess

def fatal_err(msg):
    print(f"\033[31mError: {msg}\033[0m")
    exit(1)

def run_cmd_checked(*args, **kwargs):
    """Run a command, throwing an exception if it exits with non-zero status."""
    kwargs["check"] = True
    return subprocess.run(*args, **kwargs)

def find_project_root():
    """Find the absolute path of the project repository root."""
    cur_dir = Path(__file__).parent
    while not Path(cur_dir, "LICENSE").exists():
        cur_dir = cur_dir.parent
    return cur_dir.absolute()

LAST_CONTENTS_HASH_FILE = ".lastAutoPublishContentsHash"

GITIGNORED_FILES_THAT_AFFECT_THE_BUILD = ["local.properties"]

parser = argparse.ArgumentParser(description="Publish android packages to local maven repo, but only if changed since last publish")
parser.parse_args()

root_dir = find_project_root()
if str(root_dir) != os.path.abspath(os.curdir):
    fatal_err(f"This only works if run from the repo root ({root_dir!r} != {os.path.abspath(os.curdir)!r})")

# Calculate a hash reflecting the current state of the repo.

contents_hash = hashlib.sha256()

contents_hash.update(
    run_cmd_checked(["git", "rev-parse", "HEAD"], capture_output=True).stdout
)
contents_hash.update(b"\x00")

# Git can efficiently tell us about changes to tracked files, including
# the diff of their contents, if you give it enough "-v"s.

changes = run_cmd_checked(["git", "status", "-v", "-v"], capture_output=True).stdout
contents_hash.update(changes)
contents_hash.update(b"\x00")

# But unfortunately it can only tell us the names of untracked
# files, and it won't tell us anything about files that are in
# .gitignore but can still affect the build.

untracked_files = []

# First, get a list of all untracked files sans standard exclusions.

# -o is for getting other (i.e. untracked) files
# --exclude-standard is to handle standard Git exclusions: .git/info/exclude, .gitignore in each directory,
# and the user's global exclusion file.
changes_others = run_cmd_checked(["git", "ls-files", "-o", "--exclude-standard"], capture_output=True).stdout
changes_lines = iter(ln.strip() for ln in changes_others.split(b"\n"))

try:
    ln = next(changes_lines)
    while ln:
        untracked_files.append(ln)
        ln = next(changes_lines)
except StopIteration:
    pass

# Then, account for some excluded files that we care about.
untracked_files.extend(GITIGNORED_FILES_THAT_AFFECT_THE_BUILD)

# Finally, get hashes of everything.
# Skip files that don't exist, e.g. missing GITIGNORED_FILES_THAT_AFFECT_THE_BUILD. `hash-object` errors out if it gets
# a non-existent file, so we hope that disk won't change between this filter and the cmd run just below.
filtered_untracked = [nm for nm in untracked_files if os.path.isfile(nm)]
# Reading contents of the files is quite slow when there are lots of them, so delegate to `git hash-object`.
git_hash_object_cmd = ["git", "hash-object"]
git_hash_object_cmd.extend(filtered_untracked)
changes_untracked = run_cmd_checked(git_hash_object_cmd, capture_output=True).stdout
contents_hash.update(changes_untracked)
contents_hash.update(b"\x00")

contents_hash = contents_hash.hexdigest()

# If the contents hash has changed since last publish, re-publish.
last_contents_hash = ""
try:
    with open(LAST_CONTENTS_HASH_FILE) as f:
        last_contents_hash = f.read().strip()
except FileNotFoundError:
    pass

if contents_hash == last_contents_hash:
    print("Contents have not changed, no need to publish")
else:
    print("Contents have changed, publishing")
    if sys.platform.startswith("win"):
        run_cmd_checked(["gradlew.bat", "publishToMavenLocal", f"-Plocal={time.time_ns()}"], shell=True)
    else:
        run_cmd_checked(["./gradlew", "publishToMavenLocal", f"-Plocal={time.time_ns()}"])
    with open(LAST_CONTENTS_HASH_FILE, "w") as f:
        f.write(contents_hash)
        f.write("\n")
