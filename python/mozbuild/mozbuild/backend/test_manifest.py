# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import cPickle as pickle
from collections import defaultdict

import mozpack.path as mozpath

from mozbuild.backend.base import PartialBackend
from mozbuild.frontend.data import TestManifest


class TestManifestBackend(PartialBackend):
    """Partial backend that generates test metadata files."""

    def _init(self):
        self.tests_by_path = defaultdict(list)
        self.installs_by_path = defaultdict(list)
        self.deferred_installs = set()
        self.manifest_defaults = {}

        # Add config.status so performing a build will invalidate this backend.
        self.backend_input_files.add(mozpath.join(
            self.environment.topobjdir, 'config.status'))

    def consume_object(self, obj):
        if not isinstance(obj, TestManifest):
            return

        self.backend_input_files.add(obj.path)
        self.backend_input_files |= obj.context_all_paths
        for source in obj.source_relpaths:
            self.backend_input_files.add(mozpath.join(obj.topsrcdir,
                                                      source))
        try:
            from reftest import ReftestManifest

            if isinstance(obj.manifest, ReftestManifest):
                # Mark included files as part of the build backend so changes
                # result in re-config.
                self.backend_input_files |= obj.manifest.manifests
        except ImportError:
            # Ignore errors caused by the reftest module not being present.
            # This can happen when building SpiderMonkey standalone, for example.
            pass

        for test in obj.tests:
            self.add(test, obj.flavor, obj.topsrcdir)
        self.add_defaults(obj.manifest)
        self.add_installs(obj, obj.topsrcdir)

    def consume_finished(self):
        topobjdir = self.environment.topobjdir

        with self._write_file(mozpath.join(topobjdir, 'all-tests.pkl'), mode='rb') as fh:
            pickle.dump(dict(self.tests_by_path), fh, protocol=2)

        with self._write_file(mozpath.join(topobjdir, 'test-defaults.pkl'), mode='rb') as fh:
            pickle.dump(self.manifest_defaults, fh, protocol=2)

        path = mozpath.join(topobjdir, 'test-installs.pkl')
        with self._write_file(path, mode='rb') as fh:
            pickle.dump({k: v for k, v in self.installs_by_path.items()
                         if k in self.deferred_installs},
                        fh,
                        protocol=2)

    def add(self, t, flavor, topsrcdir):
        t = dict(t)
        t['flavor'] = flavor

        path = mozpath.normpath(t['path'])
        assert mozpath.basedir(path, [topsrcdir])

        key = path[len(topsrcdir)+1:]
        t['file_relpath'] = key
        t['dir_relpath'] = mozpath.dirname(key)

        self.tests_by_path[key].append(t)

    def add_defaults(self, manifest):
        if not hasattr(manifest, 'manifest_defaults'):
            return
        for sub_manifest, defaults in manifest.manifest_defaults.items():
            self.manifest_defaults[sub_manifest] = defaults

    def add_installs(self, obj, topsrcdir):
        for src, (dest, _) in obj.installs.iteritems():
            key = src[len(topsrcdir)+1:]
            self.installs_by_path[key].append((src, dest))
        for src, pat, dest in obj.pattern_installs:
            key = mozpath.join(src[len(topsrcdir)+1:], pat)
            self.installs_by_path[key].append((src, pat, dest))
        for path in obj.deferred_installs:
            self.deferred_installs.add(path[2:])
