# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from compare_locales import mozpath
from .files import ProjectFiles, REFERENCE_LOCALE
from .ini import (
    L10nConfigParser, SourceTreeConfigParser,
    EnumerateApp, EnumerateSourceTreeApp,
)
from .matcher import Matcher
from .project import ProjectConfig
from .configparser import TOMLParser, ConfigNotFound


__all__ = [
    'Matcher',
    'ProjectConfig',
    'L10nConfigParser', 'SourceTreeConfigParser',
    'EnumerateApp', 'EnumerateSourceTreeApp',
    'ProjectFiles', 'REFERENCE_LOCALE',
    'TOMLParser', 'ConfigNotFound',
]


class File(object):

    def __init__(self, fullpath, file, module=None, locale=None):
        self.fullpath = fullpath
        self.file = file
        self.module = module
        self.locale = locale
        pass

    @property
    def localpath(self):
        if self.module:
            return mozpath.join(self.locale, self.module, self.file)
        return self.file

    def __hash__(self):
        return hash(self.localpath)

    def __str__(self):
        return self.fullpath

    def __eq__(self, other):
        if not isinstance(other, File):
            return False
        return vars(self) == vars(other)

    def __ne__(self, other):
        return not (self == other)
