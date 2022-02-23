# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import abc
import errno
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import Optional, Union

from mach.util import to_optional_path
from mozfile import which
from mozpack.files import FileListFinder


class MissingVCSTool(Exception):
    """Represents a failure to find a version control tool binary."""


class MissingVCSInfo(Exception):
    """Represents a general failure to resolve a VCS interface."""


class MissingConfigureInfo(MissingVCSInfo):
    """Represents error finding VCS info from configure data."""


class MissingVCSExtension(MissingVCSInfo):
    """Represents error finding a required VCS extension."""

    def __init__(self, ext):
        self.ext = ext
        msg = "Could not detect required extension '{}'".format(self.ext)
        super(MissingVCSExtension, self).__init__(msg)


class InvalidRepoPath(Exception):
    """Represents a failure to find a VCS repo at a specified path."""


class MissingUpstreamRepo(Exception):
    """Represents a failure to automatically detect an upstream repo."""


class CannotDeleteFromRootOfRepositoryException(Exception):
    """Represents that the code attempted to delete all files from the root of
    the repository, which is not permitted."""


def get_tool_path(tool: Union[str, Path]):
    tool = Path(tool)
    """Obtain the path of `tool`."""
    if tool.is_absolute() and tool.exists():
        return str(tool)

    path = to_optional_path(which(str(tool)))
    if not path:
        raise MissingVCSTool(
            f"Unable to obtain {tool} path. Try running "
            "|mach bootstrap| to ensure your environment is up to "
            "date."
        )
    return str(path)


class Repository(object):
    """A class wrapping utility methods around version control repositories.

    This class is abstract and never instantiated. Obtain an instance by
    calling a ``get_repository_*()`` helper function.

    Clients are recommended to use the object as a context manager. But not
    all methods require this.
    """

    __metaclass__ = abc.ABCMeta

    def __init__(self, path: Path, tool: str):
        self.path = str(path.resolve())
        self._tool = Path(get_tool_path(tool))
        self._version = None
        self._valid_diff_filter = ("m", "a", "d")
        self._env = os.environ.copy()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        pass

    def _run(self, *args, **runargs):
        return_codes = runargs.get("return_codes", [])

        cmd = (str(self._tool),) + args
        try:
            return subprocess.check_output(
                cmd, cwd=self.path, env=self._env, universal_newlines=True
            )
        except subprocess.CalledProcessError as e:
            if e.returncode in return_codes:
                return ""
            raise

    @property
    def tool_version(self):
        """Return the version of the VCS tool in use as a string."""
        if self._version:
            return self._version
        info = self._run("--version").strip()
        match = re.search("version ([^\+\)]+)", info)
        if not match:
            raise Exception("Unable to identify tool version.")

        self.version = match.group(1)
        return self.version

    @property
    def has_git_cinnabar(self):
        """True if the repository is using git cinnabar."""
        return False

    @abc.abstractproperty
    def name(self):
        """Name of the tool."""

    @abc.abstractproperty
    def head_ref(self):
        """Hash of HEAD revision."""

    @abc.abstractproperty
    def base_ref(self):
        """Hash of revision the current topic branch is based on."""

    @abc.abstractmethod
    def base_ref_as_hg(self):
        """Mercurial hash of revision the current topic branch is based on.

        Return None if the hg hash of the base ref could not be calculated.
        """

    @abc.abstractproperty
    def branch(self):
        """Current branch or bookmark the checkout has active."""

    @abc.abstractmethod
    def get_commit_time(self):
        """Return the Unix time of the HEAD revision."""

    @abc.abstractmethod
    def sparse_checkout_present(self):
        """Whether the working directory is using a sparse checkout.

        A sparse checkout is defined as a working directory that only
        materializes a subset of files in a given revision.

        Returns a bool.
        """

    @abc.abstractmethod
    def get_user_email(self):
        """Return the user's email address.

        If no email is configured, then None is returned.
        """

    @abc.abstractmethod
    def get_upstream(self):
        """Reference to the upstream remote."""

    @abc.abstractmethod
    def get_changed_files(self, diff_filter, mode="unstaged", rev=None):
        """Return a list of files that are changed in this repository's
        working copy.

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
        """

    @abc.abstractmethod
    def get_outgoing_files(self, diff_filter, upstream):
        """Return a list of changed files compared to upstream.

        ``diff_filter`` works the same as `get_changed_files`.
        ``upstream`` is a remote ref to compare against. If unspecified,
        this will be determined automatically. If there is no remote ref,
        a MissingUpstreamRepo exception will be raised.
        """

    @abc.abstractmethod
    def add_remove_files(self, *paths: Union[str, Path]):
        """Add and remove files under `paths` in this repository's working copy."""

    @abc.abstractmethod
    def forget_add_remove_files(self, *paths: Union[str, Path]):
        """Undo the effects of a previous add_remove_files call for `paths`."""

    @abc.abstractmethod
    def get_tracked_files_finder(self):
        """Obtain a mozpack.files.BaseFinder of managed files in the working
        directory.

        The Finder will have its list of all files in the repo cached for its
        entire lifetime, so operations on the Finder will not track with, for
        example, commits to the repo during the Finder's lifetime.
        """

    @abc.abstractmethod
    def working_directory_clean(self, untracked=False, ignored=False):
        """Determine if the working directory is free of modifications.

        Returns True if the working directory does not have any file
        modifications. False otherwise.

        By default, untracked and ignored files are not considered. If
        ``untracked`` or ``ignored`` are set, they influence the clean check
        to factor these file classes into consideration.
        """

    @abc.abstractmethod
    def clean_directory(self, path: Union[str, Path]):
        """Undo all changes (including removing new untracked files) in the
        given `path`.
        """

    @abc.abstractmethod
    def push_to_try(self, message):
        """Create a temporary commit, push it to try and clean it up
        afterwards.

        With mercurial, MissingVCSExtension will be raised if the `push-to-try`
        extension is not installed. On git, MissingVCSExtension will be raised
        if git cinnabar is not present.
        """

    @abc.abstractmethod
    def update(self, ref):
        """Update the working directory to the specified reference."""

    def commit(self, message, author=None, date=None, paths=None):
        """Create a commit using the provided commit message. The author, date,
        and files/paths to be included may also be optionally provided. The
        message, author and date arguments must be strings, and are passed as-is
        to the commit command. Multiline commit messages are supported. The
        paths argument must be None or an array of strings that represents the
        set of files and folders to include in the commit.
        """
        args = ["commit", "-m", message]
        if author is not None:
            if isinstance(self, HgRepository):
                args = args + ["--user", author]
            elif isinstance(self, GitRepository):
                args = args + ["--author", author]
            else:
                raise MissingVCSInfo("Unknown repo type")
        if date is not None:
            args = args + ["--date", date]
        if paths is not None:
            args = args + paths
        self._run(*args)


class HgRepository(Repository):
    """An implementation of `Repository` for Mercurial repositories."""

    def __init__(self, path: Path, hg="hg"):
        import hglib.client

        super(HgRepository, self).__init__(path, tool=hg)
        self._env["HGPLAIN"] = "1"

        # Setting this modifies a global variable and makes all future hglib
        # instances use this binary. Since the tool path was validated, this
        # should be OK. But ideally hglib would offer an API that defines
        # per-instance binaries.
        hglib.HGPATH = str(self._tool)

        # Without connect=False this spawns a persistent process. We want
        # the process lifetime tied to a context manager.
        self._client = hglib.client.hgclient(
            self.path, encoding="UTF-8", configs=None, connect=False
        )

    @property
    def name(self):
        return "hg"

    @property
    def head_ref(self):
        return self._run("log", "-r", ".", "-T", "{node}")

    @property
    def base_ref(self):
        return self._run("log", "-r", "last(ancestors(.) and public())", "-T", "{node}")

    def base_ref_as_hg(self):
        return self.base_ref

    @property
    def branch(self):
        bookmarks_fn = Path(self.path) / ".hg" / "bookmarks.current"
        if bookmarks_fn.exists():
            with open(bookmarks_fn) as f:
                bookmark = f.read()
                return bookmark or None

        return None

    def __enter__(self):
        if self._client.server is None:
            # The cwd if the spawned process should be the repo root to ensure
            # relative paths are normalized to it.
            old_cwd = Path.cwd()
            try:
                os.chdir(self.path)
                self._client.open()
            finally:
                os.chdir(old_cwd)

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._client.close()

    def _run(self, *args, **runargs):
        if not self._client.server:
            return super(HgRepository, self)._run(*args, **runargs)

        # hglib requires bytes on python 3
        args = [a.encode("utf-8") if not isinstance(a, bytes) else a for a in args]
        return self._client.rawcommand(args).decode("utf-8")

    def get_commit_time(self):
        newest_public_revision_time = self._run(
            "log",
            "--rev",
            "heads(ancestors(.) and not draft())",
            "--template",
            "{word(0, date|hgdate)}",
            "--limit",
            "1",
        ).strip()

        if not newest_public_revision_time:
            raise RuntimeError(
                "Unable to find a non-draft commit in this hg "
                "repository. If you created this repository from a "
                'bundle, have you done a "hg pull" from hg.mozilla.org '
                "since?"
            )

        return int(newest_public_revision_time)

    def sparse_checkout_present(self):
        # We assume a sparse checkout is enabled if the .hg/sparse file
        # has data. Strictly speaking, we should look for a requirement in
        # .hg/requires. But since the requirement is still experimental
        # as of Mercurial 4.3, it's probably more trouble than its worth
        # to verify it.
        sparse = Path(self.path) / ".hg" / "sparse"

        try:
            st = sparse.stat()
            return st.st_size > 0
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise

            return False

    def get_user_email(self):
        # Output is in the form "First Last <flast@mozilla.com>"
        username = self._run("config", "ui.username", return_codes=[0, 1])
        if not username:
            # No username is set
            return None
        match = re.search(r"<(.*)>", username)
        if not match:
            # "ui.username" doesn't follow the "Full Name <email@domain>" convention
            return None
        return match.group(1)

    def get_upstream(self):
        return "default"

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

    def get_changed_files(self, diff_filter="ADM", mode="unstaged", rev=None):
        if rev is None:
            # Use --no-status to print just the filename.
            df = self._format_diff_filter(diff_filter, for_status=True)
            return self._run("status", "--no-status", "-{}".format(df)).splitlines()
        else:
            template = self._files_template(diff_filter)
            return self._run("log", "-r", rev, "-T", template).splitlines()

    def get_outgoing_files(self, diff_filter="ADM", upstream=None):
        template = self._files_template(diff_filter)

        if not upstream:
            return self._run(
                "log", "-r", "draft() and ancestors(.)", "--template", template
            ).split()

        return self._run(
            "outgoing",
            "-r",
            ".",
            "--quiet",
            "--template",
            template,
            upstream,
            return_codes=(1,),
        ).split()

    def add_remove_files(self, *paths: Union[str, Path]):
        if not paths:
            return

        paths = [str(path) for path in paths]

        args = ["addremove"] + paths
        m = re.search(r"\d+\.\d+", self.tool_version)
        simplified_version = float(m.group(0)) if m else 0
        if simplified_version >= 3.9:
            args = ["--config", "extensions.automv="] + args
        self._run(*args)

    def forget_add_remove_files(self, *paths: Union[str, Path]):
        if not paths:
            return

        paths = [str(path) for path in paths]

        self._run("forget", *paths)

    def get_tracked_files_finder(self):
        # Can return backslashes on Windows. Normalize to forward slashes.
        files = list(
            p.replace("\\", "/") for p in self._run("files", "-0").split("\0") if p
        )
        return FileListFinder(files)

    def working_directory_clean(self, untracked=False, ignored=False):
        args = ["status", "--modified", "--added", "--removed", "--deleted"]
        if untracked:
            args.append("--unknown")
        if ignored:
            args.append("--ignored")

        # If output is empty, there are no entries of requested status, which
        # means we are clean.
        return not len(self._run(*args).strip())

    def clean_directory(self, path: Union[str, Path]):
        if Path(self.path).samefile(path):
            raise CannotDeleteFromRootOfRepositoryException()
        self._run("revert", str(path))
        for single_path in self._run("st", "-un", str(path)).splitlines():
            single_path = Path(single_path)
            if single_path.is_file():
                single_path.unlink()
            else:
                shutil.rmtree(str(single_path))

    def update(self, ref):
        return self._run("update", "--check", ref)

    def push_to_try(self, message):
        try:
            subprocess.check_call(
                (str(self._tool), "push-to-try", "-m", message),
                cwd=self.path,
                env=self._env,
            )
        except subprocess.CalledProcessError:
            try:
                self._run("showconfig", "extensions.push-to-try")
            except subprocess.CalledProcessError:
                raise MissingVCSExtension("push-to-try")
            raise
        finally:
            self._run("revert", "-a")


class GitRepository(Repository):
    """An implementation of `Repository` for Git repositories."""

    def __init__(self, path: Path, git="git"):
        super(GitRepository, self).__init__(path, tool=git)

    @property
    def name(self):
        return "git"

    @property
    def head_ref(self):
        return self._run("rev-parse", "HEAD").strip()

    @property
    def base_ref(self):
        refs = self._run(
            "rev-list", "HEAD", "--topo-order", "--boundary", "--not", "--remotes"
        ).splitlines()
        if refs:
            return refs[-1][1:]  # boundary starts with a prefix `-`
        return self.head_ref

    def base_ref_as_hg(self):
        base_ref = self.base_ref
        try:
            return self._run("cinnabar", "git2hg", base_ref)
        except subprocess.CalledProcessError:
            return

    @property
    def branch(self):
        return self._run("branch", "--show-current").strip() or None

    @property
    def has_git_cinnabar(self):
        try:
            self._run("cinnabar", "--version")
        except subprocess.CalledProcessError:
            return False
        return True

    def get_commit_time(self):
        return int(self._run("log", "-1", "--format=%ct").strip())

    def sparse_checkout_present(self):
        # Not yet implemented.
        return False

    def get_user_email(self):
        email = self._run("config", "user.email", return_codes=[0, 1])
        if not email:
            return None
        return email.strip()

    def get_upstream(self):
        ref = self._run("symbolic-ref", "-q", "HEAD").strip()
        upstream = self._run("for-each-ref", "--format=%(upstream:short)", ref).strip()

        if not upstream:
            raise MissingUpstreamRepo("Could not detect an upstream repository.")

        return upstream

    def get_changed_files(self, diff_filter="ADM", mode="unstaged", rev=None):
        assert all(f.lower() in self._valid_diff_filter for f in diff_filter)

        if rev is None:
            cmd = ["diff"]
            if mode == "staged":
                cmd.append("--cached")
            elif mode == "all":
                cmd.append("HEAD")
        else:
            cmd = ["diff-tree", "-r", "--no-commit-id", rev]

        cmd.append("--name-only")
        cmd.append("--diff-filter=" + diff_filter.upper())

        return self._run(*cmd).splitlines()

    def get_outgoing_files(self, diff_filter="ADM", upstream=None):
        assert all(f.lower() in self._valid_diff_filter for f in diff_filter)

        not_condition = upstream if upstream else "--remotes"

        files = self._run(
            "log",
            "--name-only",
            "--diff-filter={}".format(diff_filter.upper()),
            "--oneline",
            "--pretty=format:",
            "HEAD",
            "--not",
            not_condition,
        ).splitlines()
        return [f for f in files if f]

    def add_remove_files(self, *paths: Union[str, Path]):
        if not paths:
            return

        paths = [str(path) for path in paths]

        self._run("add", *paths)

    def forget_add_remove_files(self, *paths: Union[str, Path]):
        if not paths:
            return

        paths = [str(path) for path in paths]

        self._run("reset", *paths)

    def get_tracked_files_finder(self):
        files = [p for p in self._run("ls-files", "-z").split("\0") if p]
        return FileListFinder(files)

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

        return not len(self._run(*args).strip())

    def clean_directory(self, path: Union[str, Path]):
        if Path(self.path).samefile(path):
            raise CannotDeleteFromRootOfRepositoryException()
        self._run("checkout", "--", str(path))
        self._run("clean", "-df", str(path))

    def update(self, ref):
        self._run("checkout", ref)

    def push_to_try(self, message):
        if not self.has_git_cinnabar:
            raise MissingVCSExtension("cinnabar")

        self._run(
            "-c", "commit.gpgSign=false", "commit", "--allow-empty", "-m", message
        )
        try:
            subprocess.check_call(
                (
                    str(self._tool),
                    "push",
                    "hg::ssh://hg.mozilla.org/try",
                    "+HEAD:refs/heads/branches/default/tip",
                ),
                cwd=self.path,
            )
        finally:
            self._run("reset", "HEAD~")

    def set_config(self, name, value):
        self._run("config", name, value)


def get_repository_object(path: Optional[Union[str, Path]], hg="hg", git="git"):
    """Get a repository object for the repository at `path`.
    If `path` is not a known VCS repository, raise an exception.
    """
    # If we provide a path to hg that does not match the on-disk casing (e.g.,
    # because `path` was normcased), then the hg fsmonitor extension will call
    # watchman with that path and watchman will spew errors.
    path = Path(path).resolve()
    if (path / ".hg").is_dir():
        return HgRepository(path, hg=hg)
    elif (path / ".git").exists():
        return GitRepository(path, git=git)
    else:
        raise InvalidRepoPath(f"Unknown VCS, or not a source checkout: {path}")


def get_repository_from_build_config(config):
    """Obtain a repository from the build configuration.

    Accepts an object that has a ``topsrcdir`` and ``subst`` attribute.
    """
    flavor = config.substs.get("VCS_CHECKOUT_TYPE")

    # If in build mode, only use what configure found. That way we ensure
    # that everything in the build system can be controlled via configure.
    if not flavor:
        raise MissingConfigureInfo(
            "could not find VCS_CHECKOUT_TYPE "
            "in build config; check configure "
            "output and verify it could find a "
            "VCS binary"
        )

    if flavor == "hg":
        return HgRepository(Path(config.topsrcdir), hg=config.substs["HG"])
    elif flavor == "git":
        return GitRepository(Path(config.topsrcdir), git=config.substs["GIT"])
    else:
        raise MissingVCSInfo("unknown VCS_CHECKOUT_TYPE value: %s" % flavor)


def get_repository_from_env():
    """Obtain a repository object by looking at the environment.

    If inside a build environment (denoted by presence of a ``buildconfig``
    module), VCS info is obtained from it, as found via configure. This allows
    us to respect what was passed into configure. Otherwise, we fall back to
    scanning the filesystem.
    """
    try:
        import buildconfig

        return get_repository_from_build_config(buildconfig)
    except (ImportError, MissingVCSTool):
        pass

    paths_to_check = [Path.cwd(), *Path.cwd().parents]

    for path in paths_to_check:
        try:
            return get_repository_object(path)
        except InvalidRepoPath:
            continue

    raise MissingVCSInfo(f"Could not find Mercurial or Git checkout for {Path.cwd()}")
