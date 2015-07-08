import os
import shutil
import subprocess
import sys
import unittest

import mozharness.base.log as log
import mozharness.base.script as script
import mozharness.mozilla.l10n.locales as locales

ALL_LOCALES = ['ar', 'be', 'de', 'es-ES']

MH_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def cleanup():
    if os.path.exists('test_logs'):
        shutil.rmtree('test_logs')

class LocalesTest(locales.LocalesMixin, script.BaseScript):
    def __init__(self, **kwargs):
        if 'config' not in kwargs:
            kwargs['config'] = {'log_type': 'simple',
                                'log_level': 'error'}
        if 'initial_config_file' not in kwargs:
            kwargs['initial_config_file'] = 'test/test.json'
        super(LocalesTest, self).__init__(**kwargs)
        self.config = {}
        self.log_obj = None

class TestLocalesMixin(unittest.TestCase):
    BASE_ABS_DIRS = ['abs_compare_locales_dir', 'abs_log_dir',
                     'abs_upload_dir', 'abs_work_dir', 'base_work_dir']
    def setUp(self):
        cleanup()

    def tearDown(self):
        cleanup()

    def test_query_locales_locales(self):
        l = LocalesTest()
        l.locales = ['a', 'b', 'c']
        self.assertEqual(l.locales, l.query_locales())

    def test_query_locales_ignore_locales(self):
        l = LocalesTest()
        l.config['locales'] = ['a', 'b', 'c']
        l.config['ignore_locales'] = ['a', 'c']
        self.assertEqual(['b'], l.query_locales())

    def test_query_locales_config(self):
        l = LocalesTest()
        l.config['locales'] = ['a', 'b', 'c']
        self.assertEqual(l.config['locales'], l.query_locales())

    def test_query_locales_json(self):
        l = LocalesTest()
        l.config['locales_file'] = os.path.join(MH_DIR, "test/helper_files/locales.json")
        l.config['base_work_dir'] = '.'
        l.config['work_dir'] = '.'
        locales = l.query_locales()
        locales.sort()
        self.assertEqual(ALL_LOCALES, locales)

# Commenting out til we can hide the FATAL ?
#    def test_query_locales_no_file(self):
#        l = LocalesTest()
#        l.config['base_work_dir'] = '.'
#        l.config['work_dir'] = '.'
#        try:
#            l.query_locales()
#        except SystemExit:
#            pass # Good
#        else:
#            self.assertTrue(False, "query_locales with no file doesn't fatal()!")

    def test_parse_locales_file(self):
        l = LocalesTest()
        self.assertEqual(ALL_LOCALES, l.parse_locales_file(os.path.join(MH_DIR, 'test/helper_files/locales.txt')))

    def _get_query_abs_dirs_obj(self):
        l = LocalesTest()
        l.config['base_work_dir'] = "base_work_dir"
        l.config['work_dir'] = "work_dir"
        return l

    def test_query_abs_dirs_base(self):
        l = self._get_query_abs_dirs_obj()
        dirs = l.query_abs_dirs().keys()
        dirs.sort()
        self.assertEqual(dirs, self.BASE_ABS_DIRS)

    def test_query_abs_dirs_base2(self):
        l = self._get_query_abs_dirs_obj()
        l.query_abs_dirs().keys()
        dirs = l.query_abs_dirs().keys()
        dirs.sort()
        self.assertEqual(dirs, self.BASE_ABS_DIRS)

    def test_query_abs_dirs_l10n(self):
        l = self._get_query_abs_dirs_obj()
        l.config['l10n_dir'] = "l10n_dir"
        dirs = l.query_abs_dirs().keys()
        dirs.sort()
        expected_dirs = self.BASE_ABS_DIRS + ['abs_l10n_dir']
        expected_dirs.sort()
        self.assertEqual(dirs, expected_dirs)

    def test_query_abs_dirs_mozilla(self):
        l = self._get_query_abs_dirs_obj()
        l.config['l10n_dir'] = "l10n_dir"
        l.config['mozilla_dir'] = "mozilla_dir"
        l.config['locales_dir'] = "locales_dir"
        dirs = l.query_abs_dirs().keys()
        dirs.sort()
        expected_dirs = self.BASE_ABS_DIRS + ['abs_mozilla_dir', 'abs_locales_src_dir', 'abs_l10n_dir']
        expected_dirs.sort()
        self.assertEqual(dirs, expected_dirs)

    def test_query_abs_dirs_objdir(self):
        l = self._get_query_abs_dirs_obj()
        l.config['l10n_dir'] = "l10n_dir"
        l.config['mozilla_dir'] = "mozilla_dir"
        l.config['locales_dir'] = "locales_dir"
        l.config['objdir'] = "objdir"
        dirs = l.query_abs_dirs().keys()
        dirs.sort()
        expected_dirs = self.BASE_ABS_DIRS + ['abs_mozilla_dir', 'abs_locales_src_dir', 'abs_l10n_dir', 'abs_objdir', 'abs_merge_dir', 'abs_locales_dir']
        expected_dirs.sort()
        self.assertEqual(dirs, expected_dirs)

if __name__ == '__main__':
    unittest.main()
