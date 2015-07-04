import os
import unittest

import mozharness.base.python as python

here = os.path.dirname(os.path.abspath(__file__))


class TestVirtualenvMixin(unittest.TestCase):
    def test_package_versions(self):
        example = os.path.join(here, 'pip-freeze.example.txt')
        output = file(example).read()
        mixin = python.VirtualenvMixin()
        packages = mixin.package_versions(output)

        # from the file
        expected = {'MakeItSo': '0.2.6',
                    'PyYAML': '3.10',
                    'Tempita': '0.5.1',
                    'WebOb': '1.2b3',
                    'coverage': '3.5.1',
                    'logilab-astng': '0.23.1',
                    'logilab-common': '0.57.1',
                    'mozdevice': '0.2',
                    'mozhttpd': '0.3',
                    'mozinfo': '0.3.3',
                    'nose': '1.1.2',
                    'pyflakes': '0.5.0',
                    'pylint': '0.25.1',
                    'virtualenv': '1.7.1.2',
                    'wsgiref': '0.1.2'}

        self.assertEqual(packages, expected)


if __name__ == '__main__':
    unittest.main()
