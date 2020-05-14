# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from collections import defaultdict
import six
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

    def leaf(self, path):
        return mozpath.relpath(path, self.root)


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


class MockOS(object):
    '''Mock `os.path.isfile` and `os.walk` based on a list of files.
    '''
    def __init__(self, root, paths):
        self.root = root
        self.files = []
        self.dirs = {}
        if not paths:
            return
        if isinstance(paths[0], six.string_types):
            paths = [
                mozpath.split(path)
                for path in sorted(paths)
            ]
        child_paths = defaultdict(list)
        for segs in paths:
            if len(segs) == 1:
                self.files.append(segs[0])
            else:
                child_paths[segs[0]].append(segs[1:])
        for root, leafs in child_paths.items():
            self.dirs[root] = MockOS(mozpath.join(self.root, root), leafs)

    def find(self, dir_path):
        relpath = mozpath.relpath(dir_path, self.root)
        if relpath.startswith('..'):
            return None
        if relpath in ('', '.'):
            return self
        segs = mozpath.split(relpath)
        node = self
        while segs:
            seg = segs.pop(0)
            if seg not in node.dirs:
                return None
            node = node.dirs[seg]
        return node

    def isfile(self, path):
        dirname = mozpath.dirname(path)
        if dirname:
            node = self.find(dirname)
        else:
            node = self
        return node and mozpath.basename(path) in node.files

    def walk(self, path=None):
        if path is None:
            node = self
        else:
            node = self.find(path)
            if node is None:
                return
        subdirs = sorted(node.dirs)
        if node.root is not None:
            yield node.root, subdirs, node.files
        for subdir in subdirs:
            child = node.dirs[subdir]
            for tpl in child.walk():
                yield tpl


class MockProjectFiles(ProjectFiles):
    def __init__(self, mocks, locale, projects, mergebase=None):
        (super(MockProjectFiles, self)
            .__init__(locale, projects, mergebase=mergebase))
        root = mozpath.commonprefix(mocks)
        files = [mozpath.relpath(f, root) for f in mocks]
        self.mocks = MockOS(root, files)

    def _isfile(self, path):
        return self.mocks.isfile(path)

    def _walk(self, base):
        base = mozpath.normpath(base)
        root = self.mocks.find(base)
        if not root:
            return
        for tpl in root.walk():
            yield tpl


class MockTOMLParser(TOMLParser):
    def __init__(self, mock_data):
        self.mock_data = mock_data

    def load(self, ctx):
        p = mozpath.basename(ctx.path)
        ctx.data = toml.loads(self.mock_data[p])
