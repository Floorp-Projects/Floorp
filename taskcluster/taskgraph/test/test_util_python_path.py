# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
from ..util import python_path


class TestObject(object):

    testClassProperty = object()


class TestPythonPath(unittest.TestCase):

    def test_find_object_no_such_module(self):
        """find_object raises ImportError for a nonexistent module"""
        self.assertRaises(ImportError, python_path.find_object, "no_such_module:someobj")

    def test_find_object_no_such_object(self):
        """find_object raises AttributeError for a nonexistent object"""
        self.assertRaises(AttributeError, python_path.find_object,
                          "taskgraph.test.test_util_python_path:NoSuchObject")

    def test_find_object_exists(self):
        """find_object finds an existing object"""
        obj = python_path.find_object(
            "taskgraph.test.test_util_python_path:TestObject.testClassProperty")
        self.assertIs(obj, TestObject.testClassProperty)
