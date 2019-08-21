# -*- coding: utf-8 -*-

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import, unicode_literals, print_function

import buildconfig
import os
import unittest
import mozunit

from mozbuild.action.node import generate, SCRIPT_ALLOWLIST
from mozbuild.nodeutil import find_node_executable
import mozpack.path as mozpath


test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, 'data', 'node')


def data(name):
    return os.path.join(test_data_path, name)


TEST_SCRIPT = data("node-test-script.js")
NONEXISTENT_TEST_SCRIPT = data("non-existent-test-script.js")


class TestNode(unittest.TestCase):
    """
    Tests for node.py.
    """

    def setUp(self):
        if not buildconfig.substs.get("NODEJS"):
            buildconfig.substs['NODEJS'] = find_node_executable()[0]
        SCRIPT_ALLOWLIST.append(TEST_SCRIPT)

    def tearDown(self):
        try:
            SCRIPT_ALLOWLIST.remove(TEST_SCRIPT)
        except Exception:
            pass

    def test_generate_no_returned_deps(self):
        deps = generate("dummy_argument", TEST_SCRIPT)

        self.assertSetEqual(deps, set([]))

    def test_generate_returns_passed_deps(self):
        deps = generate("dummy_argument", TEST_SCRIPT, "a", "b")

        self.assertSetEqual(deps, set([u"a", u"b"]))

    def test_called_process_error_handled(self):
        SCRIPT_ALLOWLIST.append(NONEXISTENT_TEST_SCRIPT)

        with self.assertRaises(SystemExit) as cm:
            generate("dummy_arg", NONEXISTENT_TEST_SCRIPT)

        self.assertEqual(cm.exception.code, 1)
        SCRIPT_ALLOWLIST.remove(NONEXISTENT_TEST_SCRIPT)

    def test_nodejs_not_set(self):
        buildconfig.substs["NODEJS"] = None

        with self.assertRaises(SystemExit) as cm:
            generate("dummy_arg", TEST_SCRIPT)

        self.assertEqual(cm.exception.code, 1)

    def test_generate_missing_allowlist_entry_exit_code(self):
        SCRIPT_ALLOWLIST.remove(TEST_SCRIPT)
        with self.assertRaises(SystemExit) as cm:
            generate("dummy_arg", TEST_SCRIPT)

        self.assertEqual(cm.exception.code, 1)


if __name__ == '__main__':
    mozunit.main()
