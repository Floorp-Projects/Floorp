# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import sys
import unittest

from mozfile.mozfile import NamedTemporaryFile

from mozunit import main

from mach.logging import LoggingManager

from mozbuild.base import (
    BuildConfig,
    MozbuildObject,
    PathArgument,
)

from mozbuild.backend.configenvironment import ConfigEnvironment



curdir = os.path.dirname(__file__)
topsrcdir = os.path.abspath(os.path.join(curdir, '..', '..', '..', '..'))
log_manager = LoggingManager()


class TestMozbuildObject(unittest.TestCase):
    def get_base(self):
        return MozbuildObject(topsrcdir, None, log_manager)

    def test_objdir_config_guess(self):
        base = self.get_base()

        with NamedTemporaryFile() as mozconfig:
            os.environ[b'MOZCONFIG'] = mozconfig.name

            self.assertIsNotNone(base.topobjdir)
            self.assertEqual(len(base.topobjdir.split()), 1)
            self.assertTrue(base.topobjdir.endswith(base._config_guess))
            self.assertTrue(os.path.isabs(base.topobjdir))
            self.assertTrue(base.topobjdir.startswith(topsrcdir))

        del os.environ[b'MOZCONFIG']

    def test_objdir_trailing_slash(self):
        """Trailing slashes in topobjdir should be removed."""
        base = self.get_base()

        with NamedTemporaryFile() as mozconfig:
            mozconfig.write('mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/foo/')
            mozconfig.flush()
            os.environ[b'MOZCONFIG'] = mozconfig.name

            self.assertEqual(base.topobjdir, os.path.join(base.topsrcdir,
                'foo'))
            self.assertTrue(base.topobjdir.endswith('foo'))

        del os.environ[b'MOZCONFIG']

    def test_config_guess(self):
        # It's difficult to test for exact values from the output of
        # config.guess because they vary depending on platform.
        base = self.get_base()
        result = base._config_guess

        self.assertIsNotNone(result)
        self.assertGreater(len(result), 0)

    @unittest.skip('Failing on buildbot (bug 853954).')
    def test_config_environment(self):
        base = self.get_base()

        ce = base.config_environment
        self.assertIsInstance(ce, ConfigEnvironment)

        self.assertEqual(base.defines, ce.defines)
        self.assertEqual(base.substs, ce.substs)

        self.assertIsInstance(base.defines, dict)
        self.assertIsInstance(base.substs, dict)

    @unittest.skip('Failing on buildbot (bug 853954).')
    def test_get_binary_path(self):
        base = self.get_base()

        platform = sys.platform

        # We should ideally use the config.status from the build. Let's install
        # a fake one.
        substs = [('MOZ_APP_NAME', 'awesomeapp')]
        if sys.platform.startswith('darwin'):
            substs.append(('OS_ARCH', 'Darwin'))
            substs.append(('BIN_SUFFIX', ''))
            substs.append(('MOZ_MACBUNDLE_NAME', 'Nightly.app'))
        elif sys.platform.startswith(('win32', 'cygwin')):
            substs.append(('OS_ARCH', 'WINNT'))
            substs.append(('BIN_SUFFIX', '.exe'))
        else:
            substs.append(('OS_ARCH', 'something'))
            substs.append(('BIN_SUFFIX', ''))

        base._config_environment = ConfigEnvironment(base.topsrcdir,
            base.topobjdir, substs=substs)

        p = base.get_binary_path('xpcshell', False)
        if platform.startswith('darwin'):
            self.assertTrue(p.endswith('Contents/MacOS/xpcshell'))
        elif platform.startswith(('win32', 'cygwin')):
            self.assertTrue(p.endswith('xpcshell.exe'))
        else:
            self.assertTrue(p.endswith('dist/bin/xpcshell'))

        p = base.get_binary_path(validate_exists=False)
        if platform.startswith('darwin'):
            self.assertTrue(p.endswith('Contents/MacOS/awesomeapp'))
        elif platform.startswith(('win32', 'cygwin')):
            self.assertTrue(p.endswith('awesomeapp.exe'))
        else:
            self.assertTrue(p.endswith('dist/bin/awesomeapp'))

        p = base.get_binary_path(validate_exists=False, where="staged-package")
        if platform.startswith('darwin'):
            self.assertTrue(p.endswith('awesomeapp/Nightly.app/Contents/MacOS/awesomeapp'))
        elif platform.startswith(('win32', 'cygwin')):
            self.assertTrue(p.endswith('awesomeapp/awesomeapp.exe'))
        else:
            self.assertTrue(p.endswith('awesomeapp/awesomeapp'))

        self.assertRaises(Exception, base.get_binary_path, where="somewhere")

        p = base.get_binary_path('foobar', validate_exists=False)
        if platform.startswith('win32'):
            self.assertTrue(p.endswith('foobar.exe'))
        else:
            self.assertTrue(p.endswith('foobar'))

class TestPathArgument(unittest.TestCase):
    def test_path_argument(self):
        # Absolute path
        p = PathArgument("/obj/foo", "/src", "/obj", "/src")
        self.assertEqual(p.relpath(), "foo")
        self.assertEqual(p.srcdir_path(), "/src/foo")
        self.assertEqual(p.objdir_path(), "/obj/foo")

        # Relative path within srcdir
        p = PathArgument("foo", "/src", "/obj", "/src")
        self.assertEqual(p.relpath(), "foo")
        self.assertEqual(p.srcdir_path(), "/src/foo")
        self.assertEqual(p.objdir_path(), "/obj/foo")

        # Relative path within subdirectory
        p = PathArgument("bar", "/src", "/obj", "/src/foo")
        self.assertEqual(p.relpath(), "foo/bar")
        self.assertEqual(p.srcdir_path(), "/src/foo/bar")
        self.assertEqual(p.objdir_path(), "/obj/foo/bar")

        # Relative path within objdir
        p = PathArgument("foo", "/src", "/obj", "/obj")
        self.assertEqual(p.relpath(), "foo")
        self.assertEqual(p.srcdir_path(), "/src/foo")
        self.assertEqual(p.objdir_path(), "/obj/foo")

        # "." path
        p = PathArgument(".", "/src", "/obj", "/src/foo")
        self.assertEqual(p.relpath(), "foo")
        self.assertEqual(p.srcdir_path(), "/src/foo")
        self.assertEqual(p.objdir_path(), "/obj/foo")

        # Nested src/obj directories
        p = PathArgument("bar", "/src", "/src/obj", "/src/obj/foo")
        self.assertEqual(p.relpath(), "foo/bar")
        self.assertEqual(p.srcdir_path(), "/src/foo/bar")
        self.assertEqual(p.objdir_path(), "/src/obj/foo/bar")

if __name__ == '__main__':
    main()
