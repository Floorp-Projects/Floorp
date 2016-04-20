# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

from mozbuild.configure import ConfigureSandbox
from mozbuild.util import ReadOnlyNamespace
from mozpack import path as mozpath

from which import WhichError


class ConfigureTestVFS(object):
    def __init__(self, paths):
        self._paths = set(mozpath.abspath(p) for p in paths)

    def exists(self, path):
        return mozpath.abspath(path) in self._paths

    def isfile(self, path):
        return mozpath.abspath(path) in self._paths


class ConfigureTestSandbox(ConfigureSandbox):
    def __init__(self, paths, config, environ, *args, **kwargs):
        self._search_path = environ.get('PATH', '').split(os.pathsep)

        vfs = ConfigureTestVFS(paths)

        self.OS = ReadOnlyNamespace(path=ReadOnlyNamespace(**{
            k: v if k not in ('exists', 'isfile')
            else getattr(vfs, k)
            for k, v in ConfigureSandbox.OS.path.__dict__.iteritems()
        }))

        super(ConfigureTestSandbox, self).__init__(config, environ, *args,
                                                   **kwargs)

    def _get_one_import(self, what):
        if what == 'which.which':
            return self.which

        if what == 'which':
            return ReadOnlyNamespace(
                which=self.which,
                WhichError=WhichError,
            )

        return super(ConfigureTestSandbox, self)._get_one_import(what)

    def which(self, command):
        for parent in self._search_path:
            path = mozpath.join(parent, command)
            if self.OS.path.exists(path):
                return path
        raise WhichError()
