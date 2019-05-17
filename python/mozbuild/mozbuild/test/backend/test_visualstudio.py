# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from xml.dom.minidom import parse
import os
import unittest

from mozbuild.backend.visualstudio import VisualStudioBackend
from mozbuild.test.backend.common import BackendTester

from mozunit import main


class TestVisualStudioBackend(BackendTester):
    @unittest.skip('Failing inconsistently in automation.')
    def test_basic(self):
        """Ensure we can consume our stub project."""

        env = self._consume('visual-studio', VisualStudioBackend)

        msvc = os.path.join(env.topobjdir, 'msvc')
        self.assertTrue(os.path.isdir(msvc))

        self.assertTrue(os.path.isfile(os.path.join(msvc, 'mozilla.sln')))
        self.assertTrue(os.path.isfile(os.path.join(msvc, 'mozilla.props')))
        self.assertTrue(os.path.isfile(os.path.join(msvc, 'mach.bat')))
        self.assertTrue(os.path.isfile(os.path.join(msvc, 'binary_my_app.vcxproj')))
        self.assertTrue(os.path.isfile(os.path.join(msvc, 'target_full.vcxproj')))
        self.assertTrue(os.path.isfile(os.path.join(msvc, 'library_dir1.vcxproj')))
        self.assertTrue(os.path.isfile(os.path.join(msvc, 'library_dir1.vcxproj.user')))

        d = parse(os.path.join(msvc, 'library_dir1.vcxproj'))
        self.assertEqual(d.documentElement.tagName, 'Project')
        els = d.getElementsByTagName('ClCompile')
        self.assertEqual(len(els), 2)

        # mozilla-config.h should be explicitly listed as an include.
        els = d.getElementsByTagName('NMakeForcedIncludes')
        self.assertEqual(len(els), 1)
        self.assertEqual(els[0].firstChild.nodeValue,
            '$(TopObjDir)\\dist\\include\\mozilla-config.h')

        # LOCAL_INCLUDES get added to the include search path.
        els = d.getElementsByTagName('NMakeIncludeSearchPath')
        self.assertEqual(len(els), 1)
        includes = els[0].firstChild.nodeValue.split(';')
        self.assertIn(os.path.normpath('$(TopSrcDir)/includeA/foo'), includes)
        self.assertIn(os.path.normpath('$(TopSrcDir)/dir1'), includes)
        self.assertIn(os.path.normpath('$(TopObjDir)/dir1'), includes)
        self.assertIn(os.path.normpath('$(TopObjDir)\\dist\\include'), includes)

        # DEFINES get added to the project.
        els = d.getElementsByTagName('NMakePreprocessorDefinitions')
        self.assertEqual(len(els), 1)
        defines = els[0].firstChild.nodeValue.split(';')
        self.assertIn('DEFINEFOO', defines)
        self.assertIn('DEFINEBAR=bar', defines)


if __name__ == '__main__':
    main()
