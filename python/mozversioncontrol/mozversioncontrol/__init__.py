# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re
import subprocess
import which

from distutils.version import LooseVersion

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
        except which.WhichError as e:
            print(e)

    raise Exception('Unable to obtain %s path. Try running '
                    '|mach bootstrap| to ensure your environment is up to '
                    'date.' % tool)

class Repository(object):
    '''A class wrapping utility methods around version control repositories.'''
    def __init__(self, path, tool):
        self.path = os.path.abspath(path)
        self._tool = get_tool_path(tool)
        self._env = os.environ.copy()
        self._version = None

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

    def get_modified_files(self):
        '''Return a list of files that are modified in this repository's
        working copy.'''
        raise NotImplementedError

    def get_added_files(self):
        '''Return a list of files that are added in this repository's
        working copy.'''
        raise NotImplementedError

    def add_remove_files(self, path):
        '''Add and remove files under `path` in this repository's working copy.
        '''
        raise NotImplementedError

    def forget_add_remove_files(self, path):
        '''Undo the effects of a previous add_remove_files call for `path`.
        '''
        raise NotImplementedError

    def get_files_in_working_directory(self):
        """Obtain a list of managed files in the working directory."""
        raise NotImplementedError


class HgRepository(Repository):
    '''An implementation of `Repository` for Mercurial repositories.'''
    def __init__(self, path, hg='hg'):
        super(HgRepository, self).__init__(path, tool=hg)
        self._env[b'HGPLAIN'] = b'1'

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
                    self._run('files', '-0').split('\0'))


class GitRepository(Repository):
    '''An implementation of `Repository` for Git repositories.'''
    def __init__(self, path, git='git'):
        super(GitRepository, self).__init__(path, tool=git)

    def get_modified_files(self):
        return self._run('diff', '--diff-filter=M', '--name-only').splitlines()

    def get_added_files(self):
        return self._run('diff', '--diff-filter=A', '--name-only').splitlines()

    def add_remove_files(self, path):
        self._run('add', path)

    def forget_add_remove_files(self, path):
        self._run('reset', path)

    def get_files_in_working_directory(self):
        return self._run('ls-files', '-z').split('\0')


class InvalidRepoPath(Exception):
    """Represents a failure to find a VCS repo at a specified path."""


def get_repository_object(path):
    '''Get a repository object for the repository at `path`.
    If `path` is not a known VCS repository, raise an exception.
    '''
    if os.path.isdir(os.path.join(path, '.hg')):
        return HgRepository(path)
    elif os.path.exists(os.path.join(path, '.git')):
        return GitRepository(path)
    else:
        raise InvalidRepoPath('Unknown VCS, or not a source checkout: %s' %
                              path)


class MissingVCSInfo(Exception):
    """Represents a general failure to resolve a VCS interface."""


class MissingConfigureInfo(MissingVCSInfo):
    """Represents error finding VCS info from configure data."""


def get_repository_from_env():
    """Obtain a repository object by looking at the environment.

    If inside a build environment (denoted by presence of a ``buildconfig``
    module), VCS info is obtained from it, as found via configure. This allows
    us to respect what was passed into configure. Otherwise, we fall back to
    scanning the filesystem.
    """
    try:
        import buildconfig

        flavor = buildconfig.substs.get('VCS_CHECKOUT_TYPE')

        # If in build mode, only use what configure found. That way we ensure
        # that everything in the build system can be controlled via configure.
        if not flavor:
            raise MissingConfigureInfo('could not find VCS_CHECKOUT_TYPE '
                                       'in build config; check configure '
                                       'output and verify it could find a '
                                       'VCS binary')

        if flavor == 'hg':
            return HgRepository(buildconfig.topsrcdir,
                                hg=buildconfig.substs['HG'])
        elif flavor == 'git':
            return GitRepository(buildconfig.topsrcdir,
                                 git=buildconfig.subst['GIT'])
        else:
            raise MissingVCSInfo('unknown VCS_CHECKOUT_TYPE value: %s' % flavor)

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
