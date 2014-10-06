# -*- coding: utf-8 -*-

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import unicode_literals

import json
import os
import unittest

import mozunit

import mozbuild.action.generate_browsersearch as generate_browsersearch

from mozfile.mozfile import (
    NamedTemporaryFile,
    TemporaryDirectory,
)

import mozpack.path as mozpath


test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, 'data')


class TestGenerateBrowserSearch(unittest.TestCase):
    """
    Unit tests for generate_browsersearch.py.
    """

    def _test_one(self, name):
        with TemporaryDirectory() as tmpdir:
            with NamedTemporaryFile(mode='rw') as temp:
                srcdir = os.path.join(test_data_path, name)

                generate_browsersearch.main([
                    '--verbose',
                    '--srcdir', srcdir,
                    temp.name])
                return json.load(temp)

    def test_valid_unicode(self):
        o = self._test_one('valid-zh-CN')
        self.assertEquals(o['default'], '百度')
        self.assertEquals(o['engines'], ['百度', 'Google'])

    def test_invalid_unicode(self):
        with self.assertRaises(UnicodeDecodeError):
            self._test_one('invalid')


if __name__ == '__main__':
    mozunit.main()
