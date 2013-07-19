# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

from shutil import rmtree

from tempfile import (
    gettempdir,
    mkdtemp,
)

from mozfile.mozfile import NamedTemporaryFile

from mozunit import main

from mozbuild.mozconfig import (
    MozconfigFindException,
    MozconfigLoadException,
    MozconfigLoader,
)


class TestMozconfigLoader(unittest.TestCase):
    def setUp(self):
        self._old_env = dict(os.environ)
        os.environ.pop('MOZCONFIG', None)
        os.environ.pop('CC', None)
        os.environ.pop('CXX', None)
        self._temp_dirs = set()

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

        for d in self._temp_dirs:
            rmtree(d)

    def get_loader(self):
        return MozconfigLoader(self.get_temp_dir())

    def get_temp_dir(self):
        d = mkdtemp()
        self._temp_dirs.add(d)

        return d

    def test_find_legacy_env(self):
        """Ensure legacy mozconfig path definitions result in error."""

        os.environ[b'MOZ_MYCONFIG'] = '/foo'

        with self.assertRaises(MozconfigFindException) as e:
            self.get_loader().find_mozconfig()

        self.assertTrue(e.exception.message.startswith('The MOZ_MYCONFIG'))

    def test_find_path_not_exist(self):
        """Ensure missing files are detected."""
        os.environ[b'MOZCONFIG'] = '/foo/bar/does/not/exist'

        with self.assertRaises(MozconfigFindException) as e:
            self.get_loader().find_mozconfig()

        self.assertIn('path that does not exist', e.exception.message)
        self.assertTrue(e.exception.message.endswith('/foo/bar/does/not/exist'))

    def test_find_path_not_file(self):
        """Ensure non-file paths are detected."""

        os.environ[b'MOZCONFIG'] = gettempdir()

        with self.assertRaises(MozconfigFindException) as e:
            self.get_loader().find_mozconfig()

        self.assertIn('refers to a non-file', e.exception.message)
        self.assertTrue(e.exception.message.endswith(gettempdir()))

    def test_find_default_files(self):
        """Ensure default paths are used when present."""
        for p in MozconfigLoader.DEFAULT_TOPSRCDIR_PATHS:
            d = self.get_temp_dir()
            path = os.path.join(d, p)

            with open(path, 'w'):
                pass

            self.assertEqual(MozconfigLoader(d).find_mozconfig(), path)

    def test_find_multiple_defaults(self):
        """Ensure we error when multiple default files are present."""
        self.assertGreater(len(MozconfigLoader.DEFAULT_TOPSRCDIR_PATHS), 1)

        d = self.get_temp_dir()
        for p in MozconfigLoader.DEFAULT_TOPSRCDIR_PATHS:
            with open(os.path.join(d, p), 'w'):
                pass

        with self.assertRaises(MozconfigFindException) as e:
            MozconfigLoader(d).find_mozconfig()

        self.assertIn('Multiple default mozconfig files present',
            e.exception.message)

    def test_find_deprecated_path_srcdir(self):
        """Ensure we error when deprecated path locations are present."""
        for p in MozconfigLoader.DEPRECATED_TOPSRCDIR_PATHS:
            d = self.get_temp_dir()
            with open(os.path.join(d, p), 'w'):
                pass

            with self.assertRaises(MozconfigFindException) as e:
                MozconfigLoader(d).find_mozconfig()

            self.assertIn('This implicit location is no longer',
                e.exception.message)
            self.assertIn(d, e.exception.message)

    def test_find_deprecated_home_paths(self):
        """Ensure we error when deprecated home directory paths are present."""

        for p in MozconfigLoader.DEPRECATED_HOME_PATHS:
            home = self.get_temp_dir()
            os.environ[b'HOME'] = home
            path = os.path.join(home, p)

            with open(path, 'w'):
                pass

            with self.assertRaises(MozconfigFindException) as e:
                self.get_loader().find_mozconfig()

            self.assertIn('This implicit location is no longer',
                e.exception.message)
            self.assertIn(path, e.exception.message)

    def test_read_no_mozconfig(self):
        # This is basically to ensure changes to defaults incur a test failure.
        result = self.get_loader().read_mozconfig()

        self.assertEqual(result, {
            'path': None,
            'topobjdir': None,
            'configure_args': None,
            'make_flags': None,
            'make_extra': None,
            'env': None,
        })

    def test_read_empty_mozconfig(self):
        with NamedTemporaryFile(mode='w') as mozconfig:
            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result['path'], mozconfig.name)
            self.assertIsNone(result['topobjdir'])
            self.assertEqual(result['configure_args'], [])
            self.assertEqual(result['make_flags'], [])
            self.assertEqual(result['make_extra'], [])

            for f in ('added', 'removed', 'modified'):
                self.assertEqual(len(result['env'][f]), 0)

            self.assertGreater(len(result['env']['unmodified']), 0)

    def test_read_capture_ac_options(self):
        """Ensures ac_add_options calls are captured."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('ac_add_options --enable-debug\n')
            mozconfig.write('ac_add_options --disable-tests --enable-foo\n')
            mozconfig.write('ac_add_options --foo="bar baz"\n')
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)
            self.assertEqual(result['configure_args'], [
                '--enable-debug', '--disable-tests', '--enable-foo',
                '--foo=bar baz'])

    def test_read_ac_options_substitution(self):
        """Ensure ac_add_options values are substituted."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('ac_add_options --foo=@TOPSRCDIR@\n')
            mozconfig.flush()

            loader = self.get_loader()
            result = loader.read_mozconfig(mozconfig.name)
            self.assertEqual(result['configure_args'], [
                '--foo=%s' % loader.topsrcdir])

    def test_read_ac_app_options(self):
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('ac_add_options --foo=@TOPSRCDIR@\n')
            mozconfig.write('ac_add_app_options app1 --bar=@TOPSRCDIR@\n')
            mozconfig.write('ac_add_app_options app2 --bar=x\n')
            mozconfig.flush()

            loader = self.get_loader()
            result = loader.read_mozconfig(mozconfig.name, moz_build_app='app1')
            self.assertEqual(result['configure_args'], [
                '--foo=%s' % loader.topsrcdir,
                '--bar=%s' % loader.topsrcdir])

            result = loader.read_mozconfig(mozconfig.name, moz_build_app='app2')
            self.assertEqual(result['configure_args'], [
                '--foo=%s' % loader.topsrcdir,
                '--bar=x'])

    def test_read_capture_mk_options(self):
        """Ensures mk_add_options calls are captured."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('mk_add_options MOZ_OBJDIR=/foo/bar\n')
            mozconfig.write('mk_add_options MOZ_MAKE_FLAGS=-j8\n')
            mozconfig.write('mk_add_options FOO="BAR BAZ"\n')
            mozconfig.write('mk_add_options BIZ=1\n')
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)
            self.assertEqual(result['topobjdir'], '/foo/bar')
            self.assertEqual(result['make_flags'], '-j8')
            self.assertEqual(result['make_extra'], ['FOO=BAR BAZ', 'BIZ=1'])

    def test_read_moz_objdir_substitution(self):
        """Ensure @TOPSRCDIR@ substitution is recognized in MOZ_OBJDIR."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/some-objdir')
            mozconfig.flush()

            loader = self.get_loader()
            result = loader.read_mozconfig(mozconfig.name)

            self.assertEqual(result['topobjdir'], '%s/some-objdir' %
                loader.topsrcdir)

    def test_read_new_variables(self):
        """New variables declared in mozconfig file are detected."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('CC=/usr/local/bin/clang\n')
            mozconfig.write('CXX=/usr/local/bin/clang++\n')
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result['env']['added'], {
                'CC': '/usr/local/bin/clang',
                'CXX': '/usr/local/bin/clang++'})

    def test_read_exported_variables(self):
        """Exported variables are caught as new variables."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('export MY_EXPORTED=woot\n')
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result['env']['added'], {
                'MY_EXPORTED': 'woot'})

    def test_read_modify_variables(self):
        """Variables modified by mozconfig are detected."""
        os.environ[b'CC'] = b'/usr/bin/gcc'

        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('CC=/usr/local/bin/clang\n')
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result['env']['modified'], {
                'CC': ('/usr/bin/gcc', '/usr/local/bin/clang')
            })

    def test_read_removed_variables(self):
        """Variables unset by the mozconfig are detected."""
        os.environ[b'CC'] = b'/usr/bin/clang'

        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('unset CC\n')
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result['env']['removed'], {
                'CC': '/usr/bin/clang'})

    def test_read_multiline_variables(self):
        """Ensure multi-line variables are captured properly."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('multi="foo\nbar"\n')
            mozconfig.write('single=1\n')
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result['env']['added'], {
                'multi': 'foo\nbar',
                'single': '1'
            })

    def test_read_topsrcdir_defined(self):
        """Ensure $topsrcdir references work as expected."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('TEST=$topsrcdir')
            mozconfig.flush()

            loader = self.get_loader()
            result = loader.read_mozconfig(mozconfig.name)

            self.assertEqual(result['env']['added']['TEST'],
                loader.topsrcdir.replace(os.sep, '/'))

    def test_read_empty_variable_value(self):
        """Ensure empty variable values are parsed properly."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('EMPTY=\n')
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertIn('EMPTY', result['env']['added'])
            self.assertEqual(result['env']['added']['EMPTY'], '')

    def test_read_load_exception(self):
        """Ensure non-0 exit codes in mozconfigs are handled properly."""
        with NamedTemporaryFile(mode='w') as mozconfig:
            mozconfig.write('echo "hello world"\n')
            mozconfig.write('exit 1\n')
            mozconfig.flush()

            with self.assertRaises(MozconfigLoadException) as e:
                self.get_loader().read_mozconfig(mozconfig.name)

            self.assertTrue(e.exception.message.startswith(
                'Evaluation of your mozconfig exited with an error'))
            self.assertEquals(e.exception.path,
                mozconfig.name.replace(os.sep, '/'))
            self.assertEquals(e.exception.output, ['hello world'])


if __name__ == '__main__':
    main()
