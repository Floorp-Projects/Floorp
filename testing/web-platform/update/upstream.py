# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import subprocess
import sys
import tempfile
from six.moves import input
from six.moves.urllib import parse as urlparse

from wptrunner.update.tree import get_unique_name
from wptrunner.update.base import Step, StepRunner, exit_clean, exit_unclean

from .tree import Commit, GitTree, Patch
from .github import GitHub


def rewrite_patch(patch, strip_dir):
    """Take a Patch and convert to a different repository by stripping a prefix from the
    file paths. Also rewrite the message to remove the bug number and reviewer, but add
    a bugzilla link in the summary.

    :param patch: the Patch to convert
    :param strip_dir: the path prefix to remove
    """

    if not strip_dir.startswith("/"):
        strip_dir = "/%s" % strip_dir

    new_diff = []
    line_starts = [
        ("diff ", True),
        ("+++ ", True),
        ("--- ", True),
        ("rename from ", False),
        ("rename to ", False),
    ]
    for line in patch.diff.split("\n"):
        for start, leading_slash in line_starts:
            strip = strip_dir if leading_slash else strip_dir[1:]
            if line.startswith(start):
                new_diff.append(line.replace(strip, "").encode("utf8"))
                break
        else:
            new_diff.append(line)

    new_diff = "\n".join(new_diff)

    assert new_diff != patch

    return Patch(patch.author, patch.email, rewrite_message(patch), new_diff)


def rewrite_message(patch):
    if patch.message.bug is not None:
        return "\n".join(
            [
                patch.message.summary,
                patch.message.body,
                "",
                "Upstreamed from https://bugzilla.mozilla.org/show_bug.cgi?id=%s [ci skip]"
                % patch.message.bug,  # noqa E501
            ]
        )

    return "\n".join(
        [patch.message.full_summary, "%s\n[ci skip]\n" % patch.message.body]
    )


class SyncToUpstream(Step):
    """Sync local changes to upstream"""

    def create(self, state):
        if not state.kwargs["upstream"]:
            return

        if not isinstance(state.local_tree, GitTree):
            self.logger.error("Cannot sync with upstream from a non-Git checkout.")
            return exit_clean

        try:
            import requests  # noqa F401
        except ImportError:
            self.logger.error(
                "Upstream sync requires the requests module to be installed"
            )
            return exit_clean

        if not state.sync_tree:
            os.makedirs(state.sync["path"])
            state.sync_tree = GitTree(root=state.sync["path"])

        kwargs = state.kwargs
        with state.push(
            ["local_tree", "sync_tree", "tests_path", "metadata_path", "sync"]
        ):
            state.token = kwargs["token"]
            runner = SyncToUpstreamRunner(self.logger, state)
            runner.run()


class GetLastSyncData(Step):
    """Find the gecko commit at which we last performed a sync with upstream and the upstream
    commit that was synced."""

    provides = ["sync_data_path", "last_sync_commit", "old_upstream_rev"]

    def create(self, state):
        self.logger.info("Looking for last sync commit")
        state.sync_data_path = os.path.join(state.metadata_path, "mozilla-sync")
        items = {}
        with open(state.sync_data_path) as f:
            for line in f.readlines():
                key, value = [item.strip() for item in line.split(":", 1)]
                items[key] = value

        state.last_sync_commit = Commit(
            state.local_tree, state.local_tree.rev_from_hg(items["local"])
        )
        state.old_upstream_rev = items["upstream"]

        if not state.local_tree.contains_commit(state.last_sync_commit):
            self.logger.error(
                "Could not find last sync commit %s" % state.last_sync_commit.sha1
            )
            return exit_clean

        self.logger.info(
            "Last sync to web-platform-tests happened in %s"
            % state.last_sync_commit.sha1
        )


class CheckoutBranch(Step):
    """Create a branch in the sync tree pointing at the last upstream sync commit
    and check it out"""

    provides = ["branch"]

    def create(self, state):
        self.logger.info("Updating sync tree from %s" % state.sync["remote_url"])
        state.branch = state.sync_tree.unique_branch_name(
            "outbound_update_%s" % state.old_upstream_rev
        )
        state.sync_tree.update(
            state.sync["remote_url"], state.sync["branch"], state.branch
        )
        state.sync_tree.checkout(state.old_upstream_rev, state.branch, force=True)


class GetBaseCommit(Step):
    """Find the latest upstream commit on the branch that we are syncing with"""

    provides = ["base_commit"]

    def create(self, state):
        state.base_commit = state.sync_tree.get_remote_sha1(
            state.sync["remote_url"], state.sync["branch"]
        )
        self.logger.debug("New base commit is %s" % state.base_commit.sha1)


class LoadCommits(Step):
    """Get a list of commits in the gecko tree that need to be upstreamed"""

    provides = ["source_commits", "has_backouts"]

    def create(self, state):
        state.source_commits = state.local_tree.log(
            state.last_sync_commit, state.tests_path
        )

        update_regexp = re.compile(
            "Bug \d+ - Update web-platform-tests to revision [0-9a-f]{40}"
        )

        state.has_backouts = False

        for i, commit in enumerate(state.source_commits[:]):
            if update_regexp.match(commit.message.text):
                # This is a previous update commit so ignore it
                state.source_commits.remove(commit)
                continue

            elif commit.message.backouts:
                # TODO: Add support for collapsing backouts
                state.has_backouts = True

            elif not commit.message.bug:
                self.logger.error(
                    "Commit %i (%s) doesn't have an associated bug number."
                    % (i + 1, commit.sha1)
                )
                return exit_unclean

        self.logger.debug("Source commits: %s" % state.source_commits)


class SelectCommits(Step):
    """Provide a UI to select which commits to upstream"""

    def create(self, state):
        while True:
            commits = state.source_commits[:]
            for i, commit in enumerate(commits):
                print("{}:\t{}".format(i, commit.message.summary))

            remove = input(
                "Provide a space-separated list of any commits numbers "
                "to remove from the list to upstream:\n"
            ).strip()
            remove_idx = set()
            for item in remove.split(" "):
                try:
                    item = int(item)
                except ValueError:
                    continue
                if item < 0 or item >= len(commits):
                    continue
                remove_idx.add(item)

            keep_commits = [
                (i, cmt) for i, cmt in enumerate(commits) if i not in remove_idx
            ]
            # TODO: consider printed removed commits
            print("Selected the following commits to keep:")
            for i, commit in keep_commits:
                print("{}:\t{}".format(i, commit.message.summary))
            confirm = input("Keep the above commits? y/n\n").strip().lower()

            if confirm == "y":
                state.source_commits = [item[1] for item in keep_commits]
                break


class MovePatches(Step):
    """Convert gecko commits into patches against upstream and commit these to the sync tree."""

    provides = ["commits_loaded"]

    def create(self, state):
        if not hasattr(state, "commits_loaded"):
            state.commits_loaded = 0

        strip_path = os.path.relpath(state.tests_path, state.local_tree.root)
        self.logger.debug("Stripping patch %s" % strip_path)

        if not hasattr(state, "patch"):
            state.patch = None

        for commit in state.source_commits[state.commits_loaded :]:
            i = state.commits_loaded + 1
            self.logger.info("Moving commit %i: %s" % (i, commit.message.full_summary))
            stripped_patch = None
            if state.patch:
                filename, stripped_patch = state.patch
                if not os.path.exists(filename):
                    stripped_patch = None
                else:
                    with open(filename) as f:
                        stripped_patch.diff = f.read()
            state.patch = None
            if not stripped_patch:
                patch = commit.export_patch(state.tests_path)
                stripped_patch = rewrite_patch(patch, strip_path)
            if not stripped_patch.diff:
                self.logger.info("Skipping empty patch")
                state.commits_loaded = i
                continue
            try:
                state.sync_tree.import_patch(stripped_patch)
            except Exception:
                with tempfile.NamedTemporaryFile(delete=False, suffix=".diff") as f:
                    f.write(stripped_patch.diff)
                    print(
                        """Patch failed to apply. Diff saved in {}
Fix this file so it applies and run with --continue""".format(
                            f.name
                        )
                    )
                    state.patch = (f.name, stripped_patch)
                    print(state.patch)
                sys.exit(1)
            state.commits_loaded = i
        input("Check for differences with upstream")


class RebaseCommits(Step):
    """Rebase commits from the current branch on top of the upstream destination branch.

    This step is particularly likely to fail if the rebase generates merge conflicts.
    In that case the conflicts can be fixed up locally and the sync process restarted
    with --continue.
    """

    def create(self, state):
        self.logger.info("Rebasing local commits")
        continue_rebase = False
        # Check if there's a rebase in progress
        if os.path.exists(
            os.path.join(state.sync_tree.root, ".git", "rebase-merge")
        ) or os.path.exists(os.path.join(state.sync_tree.root, ".git", "rebase-apply")):
            continue_rebase = True

        try:
            state.sync_tree.rebase(state.base_commit, continue_rebase=continue_rebase)
        except subprocess.CalledProcessError:
            self.logger.info(
                "Rebase failed, fix merge and run %s again with --continue"
                % sys.argv[0]
            )
            raise
        self.logger.info("Rebase successful")


class CheckRebase(Step):
    """Check if there are any commits remaining after rebase"""

    provides = ["rebased_commits"]

    def create(self, state):
        state.rebased_commits = state.sync_tree.log(state.base_commit)
        if not state.rebased_commits:
            self.logger.info("Nothing to upstream, exiting")
            return exit_clean


class MergeUpstream(Step):
    """Run steps to push local commits as seperate PRs and merge upstream."""

    provides = ["merge_index", "gh_repo"]

    def create(self, state):
        gh = GitHub(state.token)
        if "merge_index" not in state:
            state.merge_index = 0

        org, name = urlparse.urlsplit(state.sync["remote_url"]).path[1:].split("/")
        if name.endswith(".git"):
            name = name[:-4]
        state.gh_repo = gh.repo(org, name)
        for commit in state.rebased_commits[state.merge_index :]:
            with state.push(["gh_repo", "sync_tree"]):
                state.commit = commit
                pr_merger = PRMergeRunner(self.logger, state)
                rv = pr_merger.run()
                if rv is not None:
                    return rv
            state.merge_index += 1


class UpdateLastSyncData(Step):
    """Update the gecko commit at which we last performed a sync with upstream."""

    provides = []

    def create(self, state):
        self.logger.info("Updating last sync commit")
        data = {
            "local": state.local_tree.rev_to_hg(state.local_tree.rev),
            "upstream": state.sync_tree.rev,
        }
        with open(state.sync_data_path, "w") as f:
            for key, value in data.iteritems():
                f.write("%s: %s\n" % (key, value))
        # This gets added to the patch later on


class MergeLocalBranch(Step):
    """Create a local branch pointing at the commit to upstream"""

    provides = ["local_branch"]

    def create(self, state):
        branch_prefix = "sync_%s" % state.commit.sha1
        local_branch = state.sync_tree.unique_branch_name(branch_prefix)

        state.sync_tree.create_branch(local_branch, state.commit)
        state.local_branch = local_branch


class MergeRemoteBranch(Step):
    """Get an unused remote branch name to use for the PR"""

    provides = ["remote_branch"]

    def create(self, state):
        remote_branch = "sync_%s" % state.commit.sha1
        branches = [
            ref[len("refs/heads/") :]
            for sha1, ref in state.sync_tree.list_remote(state.gh_repo.url)
            if ref.startswith("refs/heads")
        ]
        state.remote_branch = get_unique_name(branches, remote_branch)


class PushUpstream(Step):
    """Push local branch to remote"""

    def create(self, state):
        self.logger.info("Pushing commit upstream")
        state.sync_tree.push(state.gh_repo.url, state.local_branch, state.remote_branch)


class CreatePR(Step):
    """Create a PR for the remote branch"""

    provides = ["pr"]

    def create(self, state):
        self.logger.info("Creating a PR")
        commit = state.commit
        state.pr = state.gh_repo.create_pr(
            commit.message.full_summary,
            state.remote_branch,
            "master",
            commit.message.body if commit.message.body else "",
        )


class PRAddComment(Step):
    """Add an issue comment indicating that the code has been reviewed already"""

    def create(self, state):
        state.pr.issue.add_comment("Code reviewed upstream.")


class MergePR(Step):
    """Merge the PR"""

    def create(self, state):
        self.logger.info("Merging PR")
        state.pr.merge()


class PRDeleteBranch(Step):
    """Delete the remote branch"""

    def create(self, state):
        self.logger.info("Deleting remote branch")
        state.sync_tree.push(state.gh_repo.url, "", state.remote_branch)


class SyncToUpstreamRunner(StepRunner):
    """Runner for syncing local changes to upstream"""

    steps = [
        GetLastSyncData,
        CheckoutBranch,
        GetBaseCommit,
        LoadCommits,
        SelectCommits,
        MovePatches,
        RebaseCommits,
        CheckRebase,
        MergeUpstream,
        UpdateLastSyncData,
    ]


class PRMergeRunner(StepRunner):
    """(Sub)Runner for creating and merging a PR"""

    steps = [
        MergeLocalBranch,
        MergeRemoteBranch,
        PushUpstream,
        CreatePR,
        PRAddComment,
        MergePR,
        PRDeleteBranch,
    ]
