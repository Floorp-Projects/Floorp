# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import subprocess
from abc import ABC, abstractmethod, abstractproperty
from shutil import which

import requests
from redo import retry

from taskgraph.util.path import ancestors

PUSHLOG_TMPL = "{}/json-pushes?version=2&changeset={}&tipsonly=1&full=1"


class Repository(ABC):
    def __init__(self, path):
        self.path = path
        self.binary = which(self.tool)
        if self.binary is None:
            raise OSError(f"{self.tool} not found!")

        self._env = os.environ.copy()

    def run(self, *args: str, **kwargs):
        cmd = (self.binary,) + args
        return subprocess.check_output(
            cmd, cwd=self.path, env=self._env, encoding="utf-8", **kwargs
        )

    @abstractproperty
    def tool(self) -> str:
        """Version control system being used, either 'hg' or 'git'."""

    @abstractproperty
    def head_ref(self) -> str:
        """Hash of HEAD revision."""

    @abstractproperty
    def base_ref(self):
        """Hash of revision the current topic branch is based on."""

    @abstractproperty
    def branch(self):
        """Current branch or bookmark the checkout has active."""

    @abstractmethod
    def get_url(self, remote=None):
        """Get URL of the upstream repository."""

    @abstractmethod
    def get_commit_message(self, revision=None):
        """Commit message of specified revision or current commit."""

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


class HgRepository(Repository):
    tool = "hg"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._env["HGPLAIN"] = "1"

    @property
    def head_ref(self):
        return self.run("log", "-r", ".", "-T", "{node}").strip()

    @property
    def base_ref(self):
        return self.run("log", "-r", "last(ancestors(.) and public())", "-T", "{node}")

    @property
    def branch(self):
        bookmarks_fn = os.path.join(self.path, ".hg", "bookmarks.current")
        if os.path.exists(bookmarks_fn):
            with open(bookmarks_fn) as f:
                bookmark = f.read()
                return bookmark or None

        return None

    def get_url(self, remote="default"):
        return self.run("path", "-T", "{url}", remote).strip()

    def get_commit_message(self, revision=None):
        revision = revision or self.head_ref
        return self.run("log", "-r", ".", "-T", "{desc}")

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


class GitRepository(Repository):
    tool = "git"

    @property
    def head_ref(self):
        return self.run("rev-parse", "--verify", "HEAD").strip()

    @property
    def base_ref(self):
        refs = self.run(
            "rev-list", "HEAD", "--topo-order", "--boundary", "--not", "--remotes"
        ).splitlines()
        if refs:
            return refs[-1][1:]  # boundary starts with a prefix `-`
        return self.head_ref

    @property
    def branch(self):
        return self.run("branch", "--show-current").strip() or None

    def get_url(self, remote="origin"):
        return self.run("remote", "get-url", remote).strip()

    def get_commit_message(self, revision=None):
        revision = revision or self.head_ref
        return self.run("log", "-n1", "--format=%B")

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
