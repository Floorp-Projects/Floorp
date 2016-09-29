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

    raise Exception('Unable to obtain %s path. Try running ' +
                    '|mach bootstrap| to ensure your environment is up to ' +
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

    def add_remove_files(self, path):
        '''Add and remove files under `path` in this repository's working copy.
        '''
        raise NotImplementedError

class HgRepository(Repository):
    '''An implementation of `Repository` for Mercurial repositories.'''
    def __init__(self, path):
        super(HgRepository, self).__init__(path, 'hg')
        self._env[b'HGPLAIN'] = b'1'

    def get_modified_files(self):
        return [line.strip().split()[1] for line in self._run('status', '--modified').splitlines()]

    def add_remove_files(self, path):
        args = ['addremove', path]
        if self.tool_version >= b'3.9':
            args = ['--config', 'extensions.automv='] + args
        self._run(*args)

class GitRepository(Repository):
    '''An implementation of `Repository` for Git repositories.'''
    def __init__(self, path):
        super(GitRepository, self).__init__(path, 'git')

    def get_modified_files(self):
        # This is a little wonky, but it's good enough for this purpose.
        return [bits[1] for bits in map(lambda line: line.strip().split(), self._run('status', '--porcelain').splitlines()) if 'M' in bits[0]]

    def add_remove_files(self, path):
        self._run('add', path)

def get_repository_object(path):
    '''Get a repository object for the repository at `path`.
    If `path` is not a known VCS repository, raise an exception.
    '''
    if os.path.isdir(os.path.join(path, '.hg')):
        return HgRepository(path)
    elif os.path.isdir(os.path.join(path, '.git')):
        return GitRepository(path)
    else:
        raise Exception('Unknown VCS, or not a source checkout: %s' % path)
