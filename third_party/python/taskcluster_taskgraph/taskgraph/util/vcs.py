# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import os
import re
import subprocess
from abc import ABC, abstractmethod, abstractproperty
from shutil import which

import requests
from redo import retry

from taskgraph.util.path import ancestors

PUSHLOG_TMPL = "{}/json-pushes?version=2&changeset={}&tipsonly=1&full=1"

logger = logging.getLogger(__name__)


class Repository(ABC):
    # Both mercurial and git use sha1 as revision idenfiers. Luckily, both define
    # the same value as the null revision.
    #
    # https://github.com/git/git/blob/dc04167d378fb29d30e1647ff6ff51dd182bc9a3/t/oid-info/hash-info#L7
    # https://www.mercurial-scm.org/repo/hg-stable/file/82efc31bd152/mercurial/node.py#l30
    NULL_REVISION = "0000000000000000000000000000000000000000"

    def __init__(self, path):
        self.path = path
        self.binary = which(self.tool)
        if self.binary is None:
            raise OSError(f"{self.tool} not found!")
        self._valid_diff_filter = ("m", "a", "d")

        self._env = os.environ.copy()

    def run(self, *args: str, **kwargs):
        return_codes = kwargs.pop("return_codes", [])
        cmd = (self.binary,) + args

        try:
            return subprocess.check_output(
                cmd, cwd=self.path, env=self._env, encoding="utf-8", **kwargs
            )
        except subprocess.CalledProcessError as e:
            if e.returncode in return_codes:
                return ""
            raise

    @abstractproperty
    def tool(self) -> str:
        """Version control system being used, either 'hg' or 'git'."""

    @abstractproperty
    def head_rev(self) -> str:
        """Hash of HEAD revision."""

    @abstractproperty
    def base_rev(self):
        """Hash of revision the current topic branch is based on."""

    @abstractproperty
    def branch(self):
        """Current branch or bookmark the checkout has active."""

    @abstractproperty
    def all_remote_names(self):
        """Name of all configured remote repositories."""

    @abstractproperty
    def default_remote_name(self):
        """Name the VCS defines for the remote repository when cloning
        it for the first time. This name may not exist anymore if users
        changed the default configuration, for instance."""

    @abstractproperty
    def remote_name(self):
        """Name of the remote repository."""

    def _get_most_suitable_remote(self, remote_instructions):
        remotes = self.all_remote_names
        if len(remotes) == 1:
            return remotes[0]

        if self.default_remote_name in remotes:
            return self.default_remote_name

        first_remote = remotes[0]
        logger.warning(
            f"Unable to determine which remote repository to use between: {remotes}. "
            f'Arbitrarily using the first one "{first_remote}". Please set an '
            f"`{self.default_remote_name}` remote if the arbitrarily selected one "
            f"is not right. To do so: {remote_instructions}"
        )

        return first_remote

    @abstractproperty
    def default_branch(self):
        """Name of the default branch."""

    @abstractmethod
    def get_url(self, remote=None):
        """Get URL of the upstream repository."""

    @abstractmethod
    def get_commit_message(self, revision=None):
        """Commit message of specified revision or current commit."""

    @abstractmethod
    def get_changed_files(self, diff_filter, mode="unstaged", rev=None, base_rev=None):
        """Return a list of files that are changed in:
         * either this repository's working copy,
         * or at a given revision (``rev``)
         * or between 2 revisions (``base_rev`` and ``rev``)

        ``diff_filter`` controls which kinds of modifications are returned.
        It is a string which may only contain the following characters:

            A - Include files that were added
            D - Include files that were deleted
            M - Include files that were modified

        By default, all three will be included.

        ``mode`` can be one of 'unstaged', 'staged' or 'all'. Only has an
        effect on git. Defaults to 'unstaged'.

        ``rev`` is a specifier for which changesets to consider for
        changes. The exact meaning depends on the vcs system being used.

        ``base_rev`` specifies the range of changesets. This parameter cannot
        be used without ``rev``. The range includes ``rev`` but excludes
        ``base_rev``.
        """

    @abstractmethod
    def get_outgoing_files(self, diff_filter, upstream):
        """Return a list of changed files compared to upstream.

        ``diff_filter`` works the same as `get_changed_files`.
        ``upstream`` is a remote ref to compare against. If unspecified,
        this will be determined automatically. If there is no remote ref,
        a MissingUpstreamRepo exception will be raised.
        """

    @abstractmethod
    def working_directory_clean(self, untracked=False, ignored=False):
        """Determine if the working directory is free of modifications.

        Returns True if the working directory does not have any file
        modifications. False otherwise.

        By default, untracked and ignored files are not considered. If
        ``untracked`` or ``ignored`` are set, they influence the clean check
        to factor these file classes into consideration.
        """

    @abstractmethod
    def update(self, ref):
        """Update the working directory to the specified reference."""

    @abstractmethod
    def find_latest_common_revision(self, base_ref_or_rev, head_rev):
        """Find the latest revision that is common to both the given
        ``head_rev`` and ``base_ref_or_rev``.

        If no common revision exists, ``Repository.NULL_REVISION`` will
        be returned."""

    @abstractmethod
    def does_revision_exist_locally(self, revision):
        """Check whether this revision exists in the local repository.

        If this function returns an unexpected value, then make sure
        the revision was fetched from the remote repository."""


class HgRepository(Repository):
    tool = "hg"
    default_remote_name = "default"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._env["HGPLAIN"] = "1"

    @property
    def head_rev(self):
        return self.run("log", "-r", ".", "-T", "{node}").strip()

    @property
    def base_rev(self):
        return self.run("log", "-r", "last(ancestors(.) and public())", "-T", "{node}")

    @property
    def branch(self):
        bookmarks_fn = os.path.join(self.path, ".hg", "bookmarks.current")
        if os.path.exists(bookmarks_fn):
            with open(bookmarks_fn) as f:
                bookmark = f.read()
                return bookmark or None

        return None

    @property
    def all_remote_names(self):
        remotes = self.run("paths", "--quiet").splitlines()
        if not remotes:
            raise RuntimeError("No remotes defined")
        return remotes

    @property
    def remote_name(self):
        return self._get_most_suitable_remote(
            "Edit .hg/hgrc and add:\n\n[paths]\ndefault = $URL",
        )

    @property
    def default_branch(self):
        # Mercurial recommends keeping "default"
        # https://www.mercurial-scm.org/wiki/StandardBranching#Don.27t_use_a_name_other_than_default_for_your_main_development_branch
        return "default"

    def get_url(self, remote="default"):
        return self.run("path", "-T", "{url}", remote).strip()

    def get_commit_message(self, revision=None):
        revision = revision or "."
        return self.run("log", "-r", revision, "-T", "{desc}")

    def _format_diff_filter(self, diff_filter, for_status=False):
        df = diff_filter.lower()
        assert all(f in self._valid_diff_filter for f in df)

        # When looking at the changes in the working directory, the hg status
        # command uses 'd' for files that have been deleted with a non-hg
        # command, and 'r' for files that have been `hg rm`ed. Use both.
        return df.replace("d", "dr") if for_status else df

    def _files_template(self, diff_filter):
        template = ""
        df = self._format_diff_filter(diff_filter)
        if "a" in df:
            template += "{file_adds % '{file}\\n'}"
        if "d" in df:
            template += "{file_dels % '{file}\\n'}"
        if "m" in df:
            template += "{file_mods % '{file}\\n'}"
        return template

    def get_changed_files(
        self, diff_filter="ADM", mode="unstaged", rev=None, base_rev=None
    ):
        if rev is None:
            if base_rev is not None:
                raise ValueError("Cannot specify `base_rev` without `rev`")
            # Use --no-status to print just the filename.
            df = self._format_diff_filter(diff_filter, for_status=True)
            return self.run("status", "--no-status", f"-{df}").splitlines()
        else:
            template = self._files_template(diff_filter)
            revision_argument = rev if base_rev is None else f"{base_rev}~-1::{rev}"
            return self.run("log", "-r", revision_argument, "-T", template).splitlines()

    def get_outgoing_files(self, diff_filter="ADM", upstream=None):
        template = self._files_template(diff_filter)

        if not upstream:
            return self.run(
                "log", "-r", "draft() and ancestors(.)", "--template", template
            ).split()

        return self.run(
            "outgoing",
            "-r",
            ".",
            "--quiet",
            "--template",
            template,
            upstream,
            return_codes=(1,),
        ).split()

    def working_directory_clean(self, untracked=False, ignored=False):
        args = ["status", "--modified", "--added", "--removed", "--deleted"]
        if untracked:
            args.append("--unknown")
        if ignored:
            args.append("--ignored")

        # If output is empty, there are no entries of requested status, which
        # means we are clean.
        return not len(self.run(*args).strip())

    def update(self, ref):
        return self.run("update", "--check", ref)

    def find_latest_common_revision(self, base_ref_or_rev, head_rev):
        ancestor = self.run(
            "log",
            "-r",
            f"last(ancestors('{base_ref_or_rev}') and ancestors('{head_rev}'))",
            "--template",
            "{node}",
        ).strip()
        return ancestor or self.NULL_REVISION

    def does_revision_exist_locally(self, revision):
        try:
            return bool(self.run("log", "-r", revision).strip())
        except subprocess.CalledProcessError as e:
            # Error code 255 comes with the message:
            # "abort: unknown revision $REVISION"
            if e.returncode == 255:
                return False
            raise


class GitRepository(Repository):
    tool = "git"
    default_remote_name = "origin"

    _LS_REMOTE_PATTERN = re.compile(r"ref:\s+refs/heads/(?P<branch_name>\S+)\s+HEAD")

    @property
    def head_rev(self):
        return self.run("rev-parse", "--verify", "HEAD").strip()

    @property
    def base_rev(self):
        refs = self.run(
            "rev-list", "HEAD", "--topo-order", "--boundary", "--not", "--remotes"
        ).splitlines()
        if refs:
            return refs[-1][1:]  # boundary starts with a prefix `-`
        return self.head_rev

    @property
    def branch(self):
        return self.run("branch", "--show-current").strip() or None

    @property
    def all_remote_names(self):
        remotes = self.run("remote").splitlines()
        if not remotes:
            raise RuntimeError("No remotes defined")
        return remotes

    @property
    def remote_name(self):
        try:
            remote_branch_name = self.run(
                "rev-parse",
                "--verify",
                "--abbrev-ref",
                "--symbolic-full-name",
                "@{u}",
                stderr=subprocess.PIPE,
            ).strip()
            return remote_branch_name.split("/")[0]
        except subprocess.CalledProcessError as e:
            # Error code 128 comes with the message:
            # "fatal: no upstream configured for branch $BRANCH"
            if e.returncode != 128:
                print(e.stderr)
                raise

        return self._get_most_suitable_remote("`git remote add origin $URL`")

    @property
    def default_branch(self):
        try:
            # this one works if the current repo was cloned from an existing
            # repo elsewhere
            return self._get_default_branch_from_cloned_metadata()
        except (subprocess.CalledProcessError, RuntimeError):
            pass

        try:
            # This call works if you have (network) access to the repo
            return self._get_default_branch_from_remote_query()
        except (subprocess.CalledProcessError, RuntimeError):
            pass

        # this one is the last resort in case the remote is not accessible and
        # the local repo is where `git init` was made
        return self._guess_default_branch()

    def _get_default_branch_from_remote_query(self):
        # This function requires network access to the repo
        remote_name = self.remote_name
        output = self.run("ls-remote", "--symref", remote_name, "HEAD")
        matches = self._LS_REMOTE_PATTERN.search(output)
        if not matches:
            raise RuntimeError(
                f'Could not find the default branch of remote repository "{remote_name}". '
                "Got: {output}"
            )

        branch_name = matches.group("branch_name")
        return f"{remote_name}/{branch_name}"

    def _get_default_branch_from_cloned_metadata(self):
        return self.run("rev-parse", "--abbrev-ref", f"{self.remote_name}/HEAD").strip()

    def _guess_default_branch(self):
        branches = [
            line.strip()
            for line in self.run(
                "branch", "--all", "--no-color", "--format=%(refname)"
            ).splitlines()
            for candidate_branch in ("main", "master", "branches/default/tip")
            if line.strip().endswith(candidate_branch)
        ]

        if len(branches) == 1:
            return branches[0]

        raise RuntimeError(f"Unable to find default branch. Got: {branches}")

    def get_url(self, remote="origin"):
        return self.run("remote", "get-url", remote).strip()

    def get_commit_message(self, revision=None):
        revision = revision or "HEAD"
        return self.run("log", "-n1", "--format=%B", revision)

    def get_changed_files(
        self, diff_filter="ADM", mode="unstaged", rev=None, base_rev=None
    ):
        assert all(f.lower() in self._valid_diff_filter for f in diff_filter)

        if rev is None:
            if base_rev is not None:
                raise ValueError("Cannot specify `base_rev` without `rev`")
            cmd = ["diff"]
            if mode == "staged":
                cmd.append("--cached")
            elif mode == "all":
                cmd.append("HEAD")
        else:
            revision_argument = (
                f"{rev}~1..{rev}" if base_rev is None else f"{base_rev}..{rev}"
            )
            cmd = ["log", "--format=format:", revision_argument]

        cmd.append("--name-only")
        cmd.append("--diff-filter=" + diff_filter.upper())

        files = self.run(*cmd).splitlines()
        return [f for f in files if f]

    def get_outgoing_files(self, diff_filter="ADM", upstream=None):
        assert all(f.lower() in self._valid_diff_filter for f in diff_filter)

        not_condition = upstream if upstream else "--remotes"

        files = self.run(
            "log",
            "--name-only",
            f"--diff-filter={diff_filter.upper()}",
            "--oneline",
            "--pretty=format:",
            "HEAD",
            "--not",
            not_condition,
        ).splitlines()
        return [f for f in files if f]

    def working_directory_clean(self, untracked=False, ignored=False):
        args = ["status", "--porcelain"]

        # Even in --porcelain mode, behavior is affected by the
        # ``status.showUntrackedFiles`` option, which means we need to be
        # explicit about how to treat untracked files.
        if untracked:
            args.append("--untracked-files=all")
        else:
            args.append("--untracked-files=no")

        if ignored:
            args.append("--ignored")

        # If output is empty, there are no entries of requested status, which
        # means we are clean.
        return not len(self.run(*args).strip())

    def update(self, ref):
        self.run("checkout", ref)

    def find_latest_common_revision(self, base_ref_or_rev, head_rev):
        try:
            return self.run("merge-base", base_ref_or_rev, head_rev).strip()
        except subprocess.CalledProcessError:
            return self.NULL_REVISION

    def does_revision_exist_locally(self, revision):
        try:
            return self.run("cat-file", "-t", revision).strip() == "commit"
        except subprocess.CalledProcessError as e:
            # Error code 128 comes with the message:
            # "git cat-file: could not get object info"
            if e.returncode == 128:
                return False
            raise


def get_repository(path):
    """Get a repository object for the repository at `path`.
    If `path` is not a known VCS repository, raise an exception.
    """
    for path in ancestors(path):
        if os.path.isdir(os.path.join(path, ".hg")):
            return HgRepository(path)
        elif os.path.exists(os.path.join(path, ".git")):
            return GitRepository(path)

    raise RuntimeError("Current directory is neither a git or hg repository")


def find_hg_revision_push_info(repository, revision):
    """Given the parameters for this action and a revision, find the
    pushlog_id of the revision."""
    pushlog_url = PUSHLOG_TMPL.format(repository, revision)

    def query_pushlog(url):
        r = requests.get(pushlog_url, timeout=60)
        r.raise_for_status()
        return r

    r = retry(
        query_pushlog,
        args=(pushlog_url,),
        attempts=5,
        sleeptime=10,
    )
    pushes = r.json()["pushes"]
    if len(pushes) != 1:
        raise RuntimeError(
            "Unable to find a single pushlog_id for {} revision {}: {}".format(
                repository, revision, pushes
            )
        )
    pushid = list(pushes.keys())[0]
    return {
        "pushdate": pushes[pushid]["date"],
        "pushid": pushid,
        "user": pushes[pushid]["user"],
    }
