# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import tempfile
from compare_locales.paths import (
    ProjectConfig, File, ProjectFiles, TOMLParser
)
from compare_locales import mozpath
import pytoml as toml


class Rooted(object):
    def setUp(self):
        # Use tempdir as self.root, that's absolute on all platforms
        self.root = mozpath.normpath(tempfile.gettempdir())

    def path(self, leaf=''):
        return self.root + leaf


class SetupMixin(object):
    def setUp(self):
        self.cfg = ProjectConfig(None)
        self.file = File(
            '/tmp/somedir/de/browser/one/two/file.ftl',
            'file.ftl',
            module='browser', locale='de')
        self.other_file = File(
            '/tmp/somedir/de/toolkit/two/one/file.ftl',
            'file.ftl',
            module='toolkit', locale='de')
        self.cfg.set_locales(['de'])


class MockNode(object):
    def __init__(self, name):
        self.name = name
        self.files = []
        self.dirs = {}

    def walk(self):
        subdirs = sorted(self.dirs)
        if self.name is not None:
            yield self.name, subdirs, self.files
        for subdir in subdirs:
            child = self.dirs[subdir]
            for tpl in child.walk():
                yield tpl


class MockProjectFiles(ProjectFiles):
    def __init__(self, mocks, locale, projects, mergebase=None):
        (super(MockProjectFiles, self)
            .__init__(locale, projects, mergebase=mergebase))
        self.mocks = mocks

    def _isfile(self, path):
        return path in self.mocks

    def _walk(self, base):
        base = mozpath.normpath(base)
        local_files = [
            mozpath.split(mozpath.relpath(f, base))
            for f in self.mocks if f.startswith(base)
        ]
        root = MockNode(base)
        for segs in local_files:
            node = root
            for n, seg in enumerate(segs[:-1]):
                if seg not in node.dirs:
                    node.dirs[seg] = MockNode('/'.join([base] + segs[:n+1]))
                node = node.dirs[seg]
            node.files.append(segs[-1])
        for tpl in root.walk():
            yield tpl


class MockTOMLParser(TOMLParser):
    def __init__(self, mock_data):
        self.mock_data = mock_data

    def load(self, ctx):
        p = mozpath.basename(ctx.path)
        ctx.data = toml.loads(self.mock_data[p])
