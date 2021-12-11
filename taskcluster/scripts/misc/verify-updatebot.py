#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

"""
The purpose of this job is to run on autoland and ensure that any commits
made by the Updatebot bot account are reproducible.  Patches that aren't
reproducible indicate some sort of error in this script, or represent
concerns about the integrity of the patch made by Updatebot.

More simply: If this job fails, any patches by Updatebot SHOULD NOT land
             because they may represent a security indicent.
"""

from __future__ import absolute_import, print_function

import re
import os
import sys
import requests
import subprocess

RE_BUG = re.compile("Bug (\d+)")
RE_COMMITMSG = re.compile("Update (.+) to new version (.+) from")


class Revision:
    def __init__(self, line):
        self.node = None
        self.author = None
        self.desc = None
        self.bug = None

        line = line.strip()
        if not line:
            return

        components = line.split(" | ")
        self.node, self.author, self.desc = components[0:3]
        try:
            self.bug = RE_BUG.search(self.desc).groups(0)[0]
        except Exception:
            pass

    def __str__(self):
        bug_str = " (No Bug)" if not self.bug else " (Bug %s)" % self.bug
        return self.node + " by " + self.author + bug_str


# ================================================================================================
# Find all commits we are hopefully landing in this push

assert os.environ.get("GECKO_HEAD_REV"), "No revision head in the environment"
assert os.environ.get("GECKO_HEAD_REPOSITORY"), "No repository in the environment"
url = "%s/json-pushes?changeset=%s&version=2" % (
    os.environ.get("GECKO_HEAD_REPOSITORY"),
    os.environ.get("GECKO_HEAD_REV"),
)
response = requests.get(url)
revisions_json = response.json()

assert len(revisions_json["pushes"]) >= 1, "Did not see a push in the autoland API"
pushid = list(revisions_json["pushes"].keys())[0]
rev_ids = revisions_json["pushes"][pushid]["changesets"]

revisions = []
for rev_id in rev_ids:
    rev_detail = subprocess.check_output(
        [
            "hg",
            "log",
            "--template",
            "{node} | {author} | {desc|firstline}\n",
            "-r",
            rev_id,
        ]
    )
    revisions.append(rev_detail.decode("utf-8"))

if not revisions:
    msg = """
Don't see any non-public revisions. This indicates that the mercurial
repositories may not be operating in an expected way, and a script
that performs security checks may fail to perform them properly.

Sheriffs: This _does not_ mean the changesets involved need to be backed
out. You can continue with your usual operation. However; if this occurs
during a mercurial upgrade, or a refactring of how we treat try/autoland
or the landing process, it means that we need to revisit the operation
of this script.

For now, we ask that you file a bug blocking 1618282 indicating the task
failed so we can investigate the circumstances that caused it to fail.
    """
    print(msg)
    sys.exit(-1)

# ================================================================================================
# Find all the Updatebot Revisions (there might be multiple updatebot
#      landings in a single push some day!)
i = 1
all_revisions = []
updatebot_revisions = []
print("There are %i revisions to be evaluated." % len(revisions))
for r in revisions:
    revision = Revision(r)
    if not revision.node:
        continue

    all_revisions.append(revision)
    print("  ", i, revision)
    i += 1

    if revision.author == "Updatebot <updatebot@mozilla.com>":
        updatebot_revisions.append(revision)
        if not revision.bug:
            raise Exception(
                "Could not find a bug for revision %s (Description: %s)"
                % (revision.node, revision.desc)
            )

# ================================================================================================
# Process each Updatebot revision
overall_failure = False
for u in updatebot_revisions:
    try:
        print("=" * 80)
        print("Processing the Updatebot revision %s for Bug %s" % (u.node, u.bug))

        try:
            target_revision = RE_COMMITMSG.search(u.desc).groups(0)[1]
        except Exception:
            print("Could not parse the bug description for the revision: %s" % u.desc)
            overall_failure = True
            continue

        # Get the moz.yaml file for the updatebot revision
        files_changed = subprocess.check_output(["hg", "status", "--change", u.node])
        files_changed = files_changed.decode("utf-8").split("\n")

        moz_yaml_file = None
        for f in files_changed:
            if "moz.yaml" in f:
                if moz_yaml_file:
                    msg = (
                        "Already had a moz.yaml file (%s) and then we found another? (%s)"
                        % (moz_yaml_file, f)
                    )
                    raise Exception(msg)
                moz_yaml_file = f[2:]

        # Find all the commits associated with this bug.
        # They should be ordered with the first commit as the first element and so on.
        all_commits_for_this_update = [r for r in all_revisions if r.bug == u.bug]

        print(
            "  Found %i commits associated with this bug."
            % len(all_commits_for_this_update)
        )

        # Grab the updatebot commit and transform it into patch form
        commitdiff = (
            subprocess.check_output(["hg", "export", u.node])
            .decode("utf-8")
            .split("\n")
        )
        start_index = 0
        for i in range(len(commitdiff)):
            if "diff --git" in commitdiff[i]:
                start_index = i
                break
        patch_diff = commitdiff[start_index:]

        # Okay, now go through in reverse order and backout all of the commits for this bug
        all_commits_reversed = all_commits_for_this_update
        all_commits_reversed.reverse()
        for c in all_commits_reversed:
            print("  Backing out", c.node)
            # hg doesn't support the ability to commit a backout without prompting the
            # user, but it does support not committing
            subprocess.check_output(["hg", "backout", c.node, "--no-commit"])
            subprocess.check_output(
                [
                    "hg",
                    "--config",
                    "ui.username=Updatebot Verifier <updatebot@mozilla.com>",
                    "commit",
                    "-m",
                    "Backed out changeset %s" % c.node,
                ]
            )

        # And now re-do the updatebot commit
        print("  Vendoring", moz_yaml_file)
        ret = subprocess.call(
            ["./mach", "vendor", "--revision", target_revision, moz_yaml_file]
        )
        if ret:
            print("  Vendoring returned code %i, but we're going to continue..." % ret)

        # And now get the diff
        recreated_diff = (
            subprocess.check_output(["hg", "diff"]).decode("utf-8").split("\n")
        )

        # Now compare it, print if needed, and return.
        this_failure = False
        if len(recreated_diff) != len(patch_diff):
            print(
                "  The recreated diff is %i lines long and the original diff is %i lines long."
                % (len(recreated_diff), len(patch_diff))
            )
            this_failure = True

        for i in range(min(len(recreated_diff), len(patch_diff))):
            if recreated_diff[i] != patch_diff[i]:
                if not this_failure:
                    print(
                        "  Identified a difference between patches, starting on line %i."
                        % i
                    )
                this_failure = True

        # Cleanup so we can go to the next one
        subprocess.check_output(["hg", "revert", "."])
        subprocess.check_output(
            [
                "hg",
                "--config",
                "extensions.strip=",
                "strip",
                "tip~" + str(len(all_commits_for_this_update) - 1),
            ]
        )

        # Now process the outcome
        if not this_failure:
            print("  This revision was recreated successfully.")
            continue

        print("Original Diff:")
        print("-" * 80)
        for l in patch_diff:
            print(l)
        print("-" * 80)
        print("Recreated Diff:")
        print("-" * 80)
        for l in recreated_diff:
            print(l)
        print("-" * 80)
        overall_failure = True
    except subprocess.CalledProcessError as e:
        print("Caught an exception when running:", e.cmd)
        print("Return Code:", e.returncode)
        print("-------")
        print("stdout:")
        print(e.stdout)
        print("-------")
        print("stderr:")
        print(e.stderr)
        print("----------------------------------------------")
        overall_failure = True

if overall_failure:
    sys.exit(1)
