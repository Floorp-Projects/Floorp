# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import json
import os
import unittest

from mozbuild.backend.android_eclipse import AndroidEclipseBackend
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import BuildReader
from mozbuild.test.backend.common import BackendTester
from mozpack.manifests import InstallManifest
from mozunit import main

import mozpack.path as mozpath

class TestAndroidEclipseBackend(BackendTester):
    def __init__(self, *args, **kwargs):
        BackendTester.__init__(self, *args, **kwargs)
        self.env = None

    def assertExists(self, *args):
        p = mozpath.join(self.env.topobjdir, 'android_eclipse', *args)
        self.assertTrue(os.path.exists(p), "Path %s exists" % p)

    def assertNotExists(self, *args):
        p = mozpath.join(self.env.topobjdir, 'android_eclipse', *args)
        self.assertFalse(os.path.exists(p), "Path %s does not exist" % p)

    def test_library_project_files(self):
        """Ensure we generate reasonable files for library projects."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        for f in ['.classpath',
                  '.project',
                  '.settings',
                  'AndroidManifest.xml',
                  'project.properties']:
            self.assertExists('library1', f)

    def test_main_project_files(self):
        """Ensure we generate reasonable files for main (non-library) projects."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        for f in ['.classpath',
                  '.externalToolBuilders',
                  '.project',
                  '.settings',
                  'pre_build.xml',
                  'post_build.xml',
                  'gen',
                  'lint.xml',
                  'project.properties']:
            self.assertExists('main1', f)

    def test_library_manifest(self):
        """Ensure we generate manifest for library projects."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        self.assertExists('library1', 'AndroidManifest.xml')

    def test_classpathentries(self):
        """Ensure we produce reasonable classpathentries."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        self.assertExists('main3', '.classpath')
        # This is brittle but simple.
        with open(mozpath.join(self.env.topobjdir, 'android_eclipse', 'main3', '.classpath'), 'rt') as fh:
            lines = fh.readlines()
        lines = [line.strip() for line in lines]
        self.assertIn('<classpathentry including="**/*.java" kind="src" path="a" />', lines)
        self.assertIn('<classpathentry excluding="b/Excludes.java|b/Excludes2.java" including="**/*.java" kind="src" path="b" />', lines)
        self.assertIn('<classpathentry including="**/*.java" kind="src" path="c"><attributes><attribute name="ignore_optional_problems" value="true" /></attributes></classpathentry>', lines)

    def test_library_project_setting(self):
        """Ensure we declare a library project correctly."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)

        self.assertExists('library1', 'project.properties')
        with open(mozpath.join(self.env.topobjdir, 'android_eclipse', 'library1', 'project.properties'), 'rt') as fh:
            lines = fh.readlines()
        lines = [line.strip() for line in lines]
        self.assertIn('android.library=true', lines)

        self.assertExists('main1', 'project.properties')
        with open(mozpath.join(self.env.topobjdir, 'android_eclipse', 'main1', 'project.properties'), 'rt') as fh:
            lines = fh.readlines()
        lines = [line.strip() for line in lines]
        self.assertNotIn('android.library=true', lines)

    def test_referenced_projects(self):
        """Ensure we reference another project correctly."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        self.assertExists('main4', '.classpath')
        # This is brittle but simple.
        with open(mozpath.join(self.env.topobjdir, 'android_eclipse', 'main4', '.classpath'), 'rt') as fh:
            lines = fh.readlines()
        lines = [line.strip() for line in lines]
        self.assertIn('<classpathentry combineaccessrules="false" kind="src" path="/library1" />', lines)

    def test_extra_jars(self):
        """Ensure we add class path entries to extra jars iff asked to."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        self.assertExists('main2', '.classpath')
        # This is brittle but simple.
        with open(mozpath.join(self.env.topobjdir, 'android_eclipse', 'main2', '.classpath'), 'rt') as fh:
            lines = fh.readlines()
        lines = [line.strip() for line in lines]
        self.assertIn('<classpathentry exported="true" kind="lib" path="%s/main2/extra.jar" />' % self.env.topsrcdir, lines)

    def test_included_projects(self):
        """Ensure we include another project correctly."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        self.assertExists('main4', 'project.properties')
        # This is brittle but simple.
        with open(mozpath.join(self.env.topobjdir, 'android_eclipse', 'main4', 'project.properties'), 'rt') as fh:
            lines = fh.readlines()
        lines = [line.strip() for line in lines]
        self.assertIn('android.library.reference.1=library2', lines)

    def assertInManifest(self, project_name, *args):
        manifest_path = mozpath.join(self.env.topobjdir, 'android_eclipse', '%s.manifest' % project_name)
        manifest = InstallManifest(manifest_path)
        for arg in args:
            self.assertIn(arg, manifest, '%s in manifest for project %s' % (arg, project_name))

    def assertNotInManifest(self, project_name, *args):
        manifest_path = mozpath.join(self.env.topobjdir, 'android_eclipse', '%s.manifest' % project_name)
        manifest = InstallManifest(manifest_path)
        for arg in args:
            self.assertNotIn(arg, manifest, '%s not in manifest for project %s' % (arg, project_name))

    def test_manifest_main_manifest(self):
        """Ensure we symlink manifest if asked to for main projects."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        self.assertInManifest('main1', 'AndroidManifest.xml')

    def test_manifest_res(self):
        """Ensure we symlink res/ iff asked to."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        self.assertInManifest('library1', 'res')
        self.assertNotInManifest('library2', 'res')

    def test_manifest_classpathentries(self):
        """Ensure we symlink classpathentries correctly."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        self.assertInManifest('main3', 'a/a', 'b', 'd/e')

    def test_manifest_assets(self):
        """Ensure we symlink assets/ iff asked to."""
        self.env = self._consume('android_eclipse', AndroidEclipseBackend)
        self.assertNotInManifest('main1', 'assets')
        self.assertInManifest('main2', 'assets')


if __name__ == '__main__':
    main()
