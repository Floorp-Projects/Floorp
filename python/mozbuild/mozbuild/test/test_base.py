# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import json
import os
import shutil
import sys
import tempfile
import unittest

from mozfile.mozfile import NamedTemporaryFile

from mozunit import main

from mach.logging import LoggingManager

from mozbuild.base import (
    MachCommandBase,
    MozbuildObject,
    PathArgument,
)

from mozbuild.backend.configenvironment import ConfigEnvironment


curdir = os.path.dirname(__file__)
topsrcdir = os.path.abspath(os.path.join(curdir, '..', '..', '..', '..'))
log_manager = LoggingManager()


class TestMozbuildObject(unittest.TestCase):
    def setUp(self):
        self._old_cwd = os.getcwd()
        self._old_env = dict(os.environ)
        os.environ.pop('MOZCONFIG', None)

    def tearDown(self):
        os.chdir(self._old_cwd)
        os.environ.clear()
        os.environ.update(self._old_env)

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

    @unittest.skip('Failing on buildbot.')
    def test_objdir_config_status(self):
        """Ensure @CONFIG_GUESS@ is handled when loading mozconfig."""
        base = self.get_base()
        guess = base._config_guess

        # There may be symlinks involved, so we use real paths to ensure
        # path consistency.
        d = os.path.realpath(tempfile.mkdtemp())
        try:
            mozconfig = os.path.join(d, 'mozconfig')
            with open(mozconfig, 'wt') as fh:
                fh.write('mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/foo/@CONFIG_GUESS@')
            print('Wrote mozconfig %s' % mozconfig)

            topobjdir = os.path.join(d, 'foo', guess)
            os.makedirs(topobjdir)

            # Create a fake topsrcdir.
            guess_path = os.path.join(d, 'build', 'autoconf', 'config.guess')
            os.makedirs(os.path.dirname(guess_path))
            shutil.copy(os.path.join(topsrcdir, 'build', 'autoconf',
                'config.guess',), guess_path)

            mozinfo = os.path.join(topobjdir, 'mozinfo.json')
            with open(mozinfo, 'wt') as fh:
                json.dump(dict(
                    topsrcdir=d,
                    mozconfig=mozconfig,
                ), fh)

            os.environ[b'MOZCONFIG'] = mozconfig
            os.chdir(topobjdir)

            obj = MozbuildObject.from_environment()

            self.assertEqual(obj.topobjdir, topobjdir)
        finally:
            shutil.rmtree(d)

    @unittest.skip('Failing on buildbot.')
    def test_relative_objdir(self):
        """Relative defined objdirs are loaded properly."""
        d = os.path.realpath(tempfile.mkdtemp())
        try:
            mozconfig = os.path.join(d, 'mozconfig')
            with open(mozconfig, 'wt') as fh:
                fh.write('mk_add_options MOZ_OBJDIR=./objdir')

            topobjdir = os.path.join(d, 'objdir')
            os.mkdir(topobjdir)

            mozinfo = os.path.join(topobjdir, 'mozinfo.json')
            with open(mozinfo, 'wt') as fh:
                json.dump(dict(
                    topsrcdir=d,
                    mozconfig=mozconfig,
                ), fh)

            os.environ[b'MOZCONFIG'] = mozconfig
            child = os.path.join(topobjdir, 'foo', 'bar')
            os.makedirs(child)
            os.chdir(child)

            obj = MozbuildObject.from_environment()

            self.assertEqual(obj.topobjdir, topobjdir)

        finally:
            shutil.rmtree(d)

    @unittest.skipIf(not hasattr(os, 'symlink'), 'symlinks not available.')
    def test_symlink_objdir(self):
        """Objdir that is a symlink is loaded properly."""
        d = os.path.realpath(tempfile.mkdtemp())
        try:
            topobjdir_real = os.path.join(d, 'objdir')
            topobjdir_link = os.path.join(d, 'objlink')

            os.mkdir(topobjdir_real)
            os.symlink(topobjdir_real, topobjdir_link)

            mozconfig = os.path.join(d, 'mozconfig')
            with open(mozconfig, 'wt') as fh:
                fh.write('mk_add_options MOZ_OBJDIR=%s' % topobjdir_link)

            mozinfo = os.path.join(topobjdir_real, 'mozinfo.json')
            with open(mozinfo, 'wt') as fh:
                json.dump(dict(
                    topsrcdir=d,
                    mozconfig=mozconfig,
                ), fh)

            os.chdir(topobjdir_link)
            obj = MozbuildObject.from_environment(detect_virtualenv_mozinfo=False)
            self.assertEqual(obj.topobjdir, topobjdir_real)

            os.chdir(topobjdir_real)
            obj = MozbuildObject.from_environment(detect_virtualenv_mozinfo=False)
            self.assertEqual(obj.topobjdir, topobjdir_real)

        finally:
            shutil.rmtree(d)

    @unittest.skip('Failed on buildbot (bug 853954)')
    def test_mach_command_base_inside_objdir(self):
        """Ensure a MachCommandBase constructed from inside the objdir works."""

        d = os.path.realpath(tempfile.mkdtemp())

        try:
            topobjdir = os.path.join(d, 'objdir')
            os.makedirs(topobjdir)

            topsrcdir = os.path.join(d, 'srcdir')
            os.makedirs(topsrcdir)

            mozinfo = os.path.join(topobjdir, 'mozinfo.json')
            with open(mozinfo, 'wt') as fh:
                json.dump(dict(
                    topsrcdir=topsrcdir,
                ), fh)

            os.chdir(topobjdir)

            class MockMachContext(object):
                pass

            context = MockMachContext()
            context.cwd = topobjdir
            context.topdir = topsrcdir
            context.settings = None
            context.log_manager = None

            o = MachCommandBase(context)

            self.assertEqual(o.topobjdir, topobjdir)
            self.assertEqual(o.topsrcdir, topsrcdir)

        finally:
            shutil.rmtree(d)

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
