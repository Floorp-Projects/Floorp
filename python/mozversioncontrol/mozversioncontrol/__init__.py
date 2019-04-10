# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import abc
import errno
import os
import re
import subprocess
import sys

from distutils.spawn import find_executable
from distutils.version import LooseVersion


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


def get_tool_path(tool):
    """Obtain the path of `tool`."""
    if os.path.isabs(tool) and os.path.exists(tool):
        return tool

    path = find_executable(tool)
    if not path:
        raise MissingVCSTool('Unable to obtain %s path. Try running '
                             '|mach bootstrap| to ensure your environment is up to '
                             'date.' % tool)
    return path


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
        self._version = None
        self._valid_diff_filter = ('m', 'a', 'd')

        if os.name == 'nt' and sys.version_info[0] == 2:
            self._env = {}
            for k, v in os.environ.iteritems():
                if isinstance(k, unicode):
                    k = k.encode('utf8')
                if isinstance(v, unicode):
                    v = v.encode('utf8')
                self._env[k] = v
        else:
            self._env = os.environ.copy()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        pass

    def _run(self, *args, **runargs):
        return_codes = runargs.get('return_codes', [])

        cmd = (self._tool,) + args
        try:
            return subprocess.check_output(cmd,
                                           cwd=self.path,
                                           env=self._env,
                                           universal_newlines=True)
        except subprocess.CalledProcessError as e:
            if e.returncode in return_codes:
                return ''
            raise

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
    def sparse_checkout_present(self):
        """Whether the working directory is using a sparse checkout.

        A sparse checkout is defined as a working directory that only
        materializes a subset of files in a given revision.

        Returns a bool.
        """

    @abc.abstractmethod
    def get_upstream(self):
        """Reference to the upstream remote."""

    @abc.abstractmethod
    def get_changed_files(self, diff_filter, mode='unstaged'):
        """Return a list of files that are changed in this repository's
        working copy.

        ``diff_filter`` controls which kinds of modifications are returned.
        It is a string which may only contain the following characters:

            A - Include files that were added
            D - Include files that were deleted
            M - Include files that were modified

        By default, all three will be included.

        ``mode`` can be one of 'unstaged', 'staged' or 'all'. Only has an
        affect on git. Defaults to 'unstaged'.
        """

    @abc.abstractmethod
    def get_outgoing_files(self, diff_filter, upstream='default'):
        """Return a list of changed files compared to upstream.

        ``diff_filter`` works the same as `get_changed_files`.
        ``upstream`` is a remote ref to compare against. If unspecified,
        this will be determined automatically. If there is no remote ref,
        a MissingUpstreamRepo exception will be raised.
        """

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

    @abc.abstractmethod
    def push_to_try(self, message):
        """Create a temporary commit, push it to try and clean it up
        afterwards.

        With mercurial, MissingVCSExtension will be raised if the `push-to-try`
        extension is not installed. On git, MissingVCSExtension will be raised
        if git cinnabar is not present.
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

    @property
    def head_ref(self):
        return self._run('log', '-r', '.', '-T', '{node}')

    @property
    def base_ref(self):
        return self._run('log', '-r', 'last(ancestors(.) and public())', '-T', '{node}')

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

    def _run(self, *args, **runargs):
        if not self._client.server:
            return super(HgRepository, self)._run(*args, **runargs)

        # hglib requires bytes on python 3
        args = [a.encode('utf-8') if not isinstance(a, bytes) else a for a in args]
        return self._client.rawcommand(args).decode('utf-8')

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

    def get_upstream(self):
        return 'default'

    def _format_diff_filter(self, diff_filter):
        df = diff_filter.lower()
        assert all(f in self._valid_diff_filter for f in df)

        # Mercurial uses 'r' to denote removed files whereas git uses 'd'.
        if 'd' in df:
            df.replace('d', 'r')

        return df.lower()

    def get_changed_files(self, diff_filter='ADM', mode='unstaged'):
        df = self._format_diff_filter(diff_filter)

        # Use --no-status to print just the filename.
        return self._run('status', '--no-status', '-{}'.format(df)).splitlines()

    def get_outgoing_files(self, diff_filter='ADM', upstream='default'):
        df = self._format_diff_filter(diff_filter)

        template = ''
        if 'a' in df:
            template += "{file_adds % '\\n{file}'}"
        if 'd' in df:
            template += "{file_dels % '\\n{file}'}"
        if 'm' in df:
            template += "{file_mods % '\\n{file}'}"

        return self._run('outgoing', '-r', '.', '--quiet',
                         '--template', template, upstream, return_codes=(1,)).split()

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
                    self._run(b'files', b'-0').split(b'\0') if p)

    def working_directory_clean(self, untracked=False, ignored=False):
        args = [b'status', b'--modified', b'--added', b'--removed',
                b'--deleted']
        if untracked:
            args.append(b'--unknown')
        if ignored:
            args.append(b'--ignored')

        # If output is empty, there are no entries of requested status, which
        # means we are clean.
        return not len(self._run(*args).strip())

    def push_to_try(self, message):
        try:
            subprocess.check_call((self._tool, 'push-to-try', '-m', message), cwd=self.path,
                                  env=self._env)
        except subprocess.CalledProcessError:
            try:
                self._run('showconfig', 'extensions.push-to-try')
            except subprocess.CalledProcessError:
                raise MissingVCSExtension('push-to-try')
            raise
        finally:
            self._run('revert', '-a')


class GitRepository(Repository):
    '''An implementation of `Repository` for Git repositories.'''
    def __init__(self, path, git='git'):
        super(GitRepository, self).__init__(path, tool=git)

    @property
    def name(self):
        return 'git'

    @property
    def head_ref(self):
        return self._run('rev-parse', 'HEAD').strip()

    @property
    def base_ref(self):
        refs = self._run('for-each-ref', 'refs/heads', 'refs/remotes',
                         '--format=%(objectname)').splitlines()
        head = self.head_ref
        if head in refs:
            refs.remove(head)
        return self._run('merge-base', 'HEAD', *refs).strip()

    @property
    def has_git_cinnabar(self):
        try:
            self._run('cinnabar', '--version')
        except subprocess.CalledProcessError:
            return False
        return True

    def sparse_checkout_present(self):
        # Not yet implemented.
        return False

    def get_upstream(self):
        ref = self._run('symbolic-ref', '-q', 'HEAD').strip()
        upstream = self._run('for-each-ref', '--format=%(upstream:short)', ref).strip()

        if not upstream:
            raise MissingUpstreamRepo("Could not detect an upstream repository.")

        return upstream

    def get_changed_files(self, diff_filter='ADM', mode='unstaged'):
        assert all(f.lower() in self._valid_diff_filter for f in diff_filter)

        cmd = ['diff', '--diff-filter={}'.format(diff_filter.upper()), '--name-only']
        if mode == 'staged':
            cmd.append('--cached')
        elif mode == 'all':
            cmd.append('HEAD')

        return self._run(*cmd).splitlines()

    def get_outgoing_files(self, diff_filter='ADM', upstream='default'):
        assert all(f.lower() in self._valid_diff_filter for f in diff_filter)

        if upstream == 'default':
            upstream = self.base_ref

        compare = '{}..HEAD'.format(upstream)
        files = self._run('log', '--name-only', '--diff-filter={}'.format(diff_filter.upper()),
                          '--oneline', '--pretty=format:', compare).splitlines()
        return [f for f in files if f]

    def add_remove_files(self, path):
        self._run('add', path)

    def forget_add_remove_files(self, path):
        self._run('reset', path)

    def get_files_in_working_directory(self):
        return self._run('ls-files', '-z').split(b'\0')

    def working_directory_clean(self, untracked=False, ignored=False):
        args = ['status', '--porcelain']

        # Even in --porcelain mode, behavior is affected by the
        # ``status.showUntrackedFiles`` option, which means we need to be
        # explicit about how to treat untracked files.
        if untracked:
            args.append('--untracked-files=all')
        else:
            args.append('--untracked-files=no')

        if ignored:
            args.append('--ignored')

        return not len(self._run(*args).strip())

    def push_to_try(self, message):
        if not self.has_git_cinnabar:
            raise MissingVCSExtension('cinnabar')

        self._run('commit', '--allow-empty', '-m', message)
        try:
            subprocess.check_call((self._tool, 'push', 'hg::ssh://hg.mozilla.org/try',
                                   '+HEAD:refs/heads/branches/default/tip'), cwd=self.path)
        finally:
            self._run('reset', 'HEAD~')


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
