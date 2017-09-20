# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import abc
import errno
import os
import re
import subprocess
import which

from distutils.version import LooseVersion


class MissingVCSTool(Exception):
    """Represents a failure to find a version control tool binary."""


def get_tool_path(tool):
    """Obtain the path of `tool`."""
    if os.path.isabs(tool) and os.path.exists(tool):
        return tool

    # We use subprocess in places, which expects a Win32 executable or
    # batch script. On some versions of MozillaBuild, we have "hg.exe",
    # "hg.bat," and "hg" (a Python script). "which" will happily return the
    # Python script, which will cause subprocess to choke. Explicitly favor
    # the Windows version over the plain script.
    try:
        return which.which(tool + '.exe')
    except which.WhichError:
        try:
            return which.which(tool)
        except which.WhichError:
            pass

    raise MissingVCSTool('Unable to obtain %s path. Try running '
                         '|mach bootstrap| to ensure your environment is up to '
                         'date.' % tool)


class Repository(object):
    """A class wrapping utility methods around version control repositories.

    This class is abstract and never instantiated. Obtain an instance by
    calling a ``get_repository_*()`` helper function.

    Clients are recommended to use the object as a context manager. But not
    all methods require this.
    """

    __metaclass__ = abc.ABCMeta

    def __init__(self, path, tool):
        self.path = os.path.abspath(path)
        self._tool = get_tool_path(tool)
        self._env = os.environ.copy()
        self._version = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        pass

    def _run(self, *args):
        return subprocess.check_output((self._tool, ) + args,
                                       cwd=self.path,
                                       env=self._env)

    @property
    def tool_version(self):
        '''Return the version of the VCS tool in use as a `LooseVersion`.'''
        if self._version:
            return self._version
        info = self._run('--version').strip()
        match = re.search('version ([^\+\)]+)', info)
        if not match:
            raise Exception('Unable to identify tool version.')

        self.version = LooseVersion(match.group(1))
        return self.version

    @abc.abstractproperty
    def name(self):
        """Name of the tool."""

    @abc.abstractmethod
    def sparse_checkout_present(self):
        """Whether the working directory is using a sparse checkout.

        A sparse checkout is defined as a working directory that only
        materializes a subset of files in a given revision.

        Returns a bool.
        """

    @abc.abstractmethod
    def get_modified_files(self):
        '''Return a list of files that are modified in this repository's
        working copy.'''

    @abc.abstractmethod
    def get_added_files(self):
        '''Return a list of files that are added in this repository's
        working copy.'''

    @abc.abstractmethod
    def add_remove_files(self, path):
        '''Add and remove files under `path` in this repository's working copy.
        '''

    @abc.abstractmethod
    def forget_add_remove_files(self, path):
        '''Undo the effects of a previous add_remove_files call for `path`.
        '''

    @abc.abstractmethod
    def get_files_in_working_directory(self):
        """Obtain a list of managed files in the working directory."""

    @abc.abstractmethod
    def working_directory_clean(self, untracked=False, ignored=False):
        """Determine if the working directory is free of modifications.

        Returns True if the working directory does not have any file
        modifications. False otherwise.

        By default, untracked and ignored files are not considered. If
        ``untracked`` or ``ignored`` are set, they influence the clean check
        to factor these file classes into consideration.
        """


class HgRepository(Repository):
    '''An implementation of `Repository` for Mercurial repositories.'''
    def __init__(self, path, hg='hg'):
        import hglib.client

        super(HgRepository, self).__init__(path, tool=hg)
        self._env[b'HGPLAIN'] = b'1'

        # Setting this modifies a global variable and makes all future hglib
        # instances use this binary. Since the tool path was validated, this
        # should be OK. But ideally hglib would offer an API that defines
        # per-instance binaries.
        hglib.HGPATH = self._tool

        # Without connect=False this spawns a persistent process. We want
        # the process lifetime tied to a context manager.
        self._client = hglib.client.hgclient(self.path, encoding=b'UTF-8',
                                             configs=None, connect=False)

    @property
    def name(self):
        return 'hg'

    def __enter__(self):
        if self._client.server is None:
            # The cwd if the spawned process should be the repo root to ensure
            # relative paths are normalized to it.
            old_cwd = os.getcwd()
            try:
                os.chdir(self.path)
                self._client.open()
            finally:
                os.chdir(old_cwd)

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._client.close()

    def _run_in_client(self, args):
        if not self._client.server:
            raise Exception('active HgRepository context manager required')

        return self._client.rawcommand(args)

    def sparse_checkout_present(self):
        # We assume a sparse checkout is enabled if the .hg/sparse file
        # has data. Strictly speaking, we should look for a requirement in
        # .hg/requires. But since the requirement is still experimental
        # as of Mercurial 4.3, it's probably more trouble than its worth
        # to verify it.
        sparse = os.path.join(self.path, '.hg', 'sparse')

        try:
            st = os.stat(sparse)
            return st.st_size > 0
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise

            return False

    def get_modified_files(self):
        # Use --no-status to print just the filename.
        return self._run('status', '--modified', '--no-status').splitlines()

    def get_added_files(self):
        # Use --no-status to print just the filename.
        return self._run('status', '--added', '--no-status').splitlines()

    def add_remove_files(self, path):
        args = ['addremove', path]
        if self.tool_version >= b'3.9':
            args = ['--config', 'extensions.automv='] + args
        self._run(*args)

    def forget_add_remove_files(self, path):
        self._run('forget', path)

    def get_files_in_working_directory(self):
        # Can return backslashes on Windows. Normalize to forward slashes.
        return list(p.replace('\\', '/') for p in
                    self._run_in_client([b'files', b'-0']).split(b'\0')
                    if p)

    def working_directory_clean(self, untracked=False, ignored=False):
        args = [b'status', b'\0', b'--modified', b'--added', b'--removed',
                b'--deleted']
        if untracked:
            args.append(b'--unknown')
        if ignored:
            args.append(b'--ignored')

        # If output is empty, there are no entries of requested status, which
        # means we are clean.
        return not len(self._run_in_client(args).strip())


class GitRepository(Repository):
    '''An implementation of `Repository` for Git repositories.'''
    def __init__(self, path, git='git'):
        super(GitRepository, self).__init__(path, tool=git)

    @property
    def name(self):
        return 'git'

    def sparse_checkout_present(self):
        # Not yet implemented.
        return False

    def get_modified_files(self):
        return self._run('diff', '--diff-filter=M', '--name-only').splitlines()

    def get_added_files(self):
        return self._run('diff', '--diff-filter=A', '--name-only').splitlines()

    def add_remove_files(self, path):
        self._run('add', path)

    def forget_add_remove_files(self, path):
        self._run('reset', path)

    def get_files_in_working_directory(self):
        return self._run('ls-files', '-z').split(b'\0')

    def working_directory_clean(self, untracked=False, ignored=False):
        args = ['status', '--porcelain']
        if untracked:
            args.append('--untracked-files')
        if ignored:
            args.append('--ignored')

        return not len(self._run(*args).strip())


class InvalidRepoPath(Exception):
    """Represents a failure to find a VCS repo at a specified path."""


def get_repository_object(path, hg='hg', git='git'):
    '''Get a repository object for the repository at `path`.
    If `path` is not a known VCS repository, raise an exception.
    '''
    if os.path.isdir(os.path.join(path, '.hg')):
        return HgRepository(path, hg=hg)
    elif os.path.exists(os.path.join(path, '.git')):
        return GitRepository(path, git=git)
    else:
        raise InvalidRepoPath('Unknown VCS, or not a source checkout: %s' %
                              path)


class MissingVCSInfo(Exception):
    """Represents a general failure to resolve a VCS interface."""


class MissingConfigureInfo(MissingVCSInfo):
    """Represents error finding VCS info from configure data."""


def get_repository_from_build_config(config):
    """Obtain a repository from the build configuration.

    Accepts an object that has a ``topsrcdir`` and ``subst`` attribute.
    """
    flavor = config.substs.get('VCS_CHECKOUT_TYPE')

    # If in build mode, only use what configure found. That way we ensure
    # that everything in the build system can be controlled via configure.
    if not flavor:
        raise MissingConfigureInfo('could not find VCS_CHECKOUT_TYPE '
                                   'in build config; check configure '
                                   'output and verify it could find a '
                                   'VCS binary')

    if flavor == 'hg':
        return HgRepository(config.topsrcdir, hg=config.substs['HG'])
    elif flavor == 'git':
        return GitRepository(config.topsrcdir, git=config.substs['GIT'])
    else:
        raise MissingVCSInfo('unknown VCS_CHECKOUT_TYPE value: %s' % flavor)


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
    except ImportError:
        pass

    def ancestors(path):
        while path:
            yield path
            path, child = os.path.split(path)
            if child == '':
                break

    for path in ancestors(os.getcwd()):
        try:
            return get_repository_object(path)
        except InvalidRepoPath:
            continue

    raise MissingVCSInfo('Could not find Mercurial or Git checkout for %s' %
                         os.getcwd())
