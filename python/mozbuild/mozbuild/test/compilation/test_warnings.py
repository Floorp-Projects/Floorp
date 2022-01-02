# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import unittest

from mozfile.mozfile import NamedTemporaryFile

from mozbuild.compilation.warnings import CompilerWarning
from mozbuild.compilation.warnings import WarningsCollector
from mozbuild.compilation.warnings import WarningsDatabase

from mozunit import main

CLANG_TESTS = [
    (
        "foobar.cpp:123:10: warning: you messed up [-Wfoo]",
        "foobar.cpp",
        123,
        10,
        "warning",
        "you messed up",
        "-Wfoo",
    ),
    (
        "c_locale_dummy.c:457:1: error: (near initialization for "
        "'full_wmonthname[0]') [clang-diagnostic-error]",
        "c_locale_dummy.c",
        457,
        1,
        "error",
        "(near initialization for 'full_wmonthname[0]')",
        "clang-diagnostic-error",
    ),
]

CURRENT_LINE = 1


def get_warning():
    global CURRENT_LINE

    w = CompilerWarning()
    w["filename"] = "/foo/bar/baz.cpp"
    w["line"] = CURRENT_LINE
    w["column"] = 12
    w["message"] = "This is irrelevant"

    CURRENT_LINE += 1

    return w


class TestCompilerWarning(unittest.TestCase):
    def test_equivalence(self):
        w1 = CompilerWarning()
        w2 = CompilerWarning()

        s = set()

        # Empty warnings should be equal.
        self.assertEqual(w1, w2)

        s.add(w1)
        s.add(w2)

        self.assertEqual(len(s), 1)

        w1["filename"] = "/foo.c"
        w2["filename"] = "/bar.c"

        self.assertNotEqual(w1, w2)

        s = set()
        s.add(w1)
        s.add(w2)

        self.assertEqual(len(s), 2)

        w1["filename"] = "/foo.c"
        w1["line"] = 5
        w2["line"] = 5

        w2["filename"] = "/foo.c"
        w1["column"] = 3
        w2["column"] = 3

        self.assertEqual(w1, w2)

    def test_comparison(self):
        w1 = CompilerWarning()
        w2 = CompilerWarning()

        w1["filename"] = "/aaa.c"
        w1["line"] = 5
        w1["column"] = 5

        w2["filename"] = "/bbb.c"
        w2["line"] = 5
        w2["column"] = 5

        self.assertLess(w1, w2)
        self.assertGreater(w2, w1)
        self.assertGreaterEqual(w2, w1)

        w2["filename"] = "/aaa.c"
        w2["line"] = 4
        w2["column"] = 6

        self.assertLess(w2, w1)
        self.assertGreater(w1, w2)
        self.assertGreaterEqual(w1, w2)

        w2["filename"] = "/aaa.c"
        w2["line"] = 5
        w2["column"] = 10

        self.assertLess(w1, w2)
        self.assertGreater(w2, w1)
        self.assertGreaterEqual(w2, w1)

        w2["filename"] = "/aaa.c"
        w2["line"] = 5
        w2["column"] = 5

        self.assertLessEqual(w1, w2)
        self.assertLessEqual(w2, w1)
        self.assertGreaterEqual(w2, w1)
        self.assertGreaterEqual(w1, w2)


class TestWarningsAndErrorsParsing(unittest.TestCase):
    def test_clang_parsing(self):
        for source, filename, line, column, diag_type, message, flag in CLANG_TESTS:
            collector = WarningsCollector(lambda w: None)
            warning = collector.process_line(source)

            self.assertIsNotNone(warning)

            self.assertEqual(warning["filename"], filename)
            self.assertEqual(warning["line"], line)
            self.assertEqual(warning["column"], column)
            self.assertEqual(warning["type"], diag_type)
            self.assertEqual(warning["message"], message)
            self.assertEqual(warning["flag"], flag)


class TestWarningsDatabase(unittest.TestCase):
    def test_basic(self):
        db = WarningsDatabase()

        self.assertEqual(len(db), 0)

        for i in range(10):
            db.insert(get_warning(), compute_hash=False)

        self.assertEqual(len(db), 10)

        warnings = list(db)
        self.assertEqual(len(warnings), 10)

    def test_hashing(self):
        """Ensure that hashing files on insert works."""
        db = WarningsDatabase()

        temp = NamedTemporaryFile(mode="wt")
        temp.write("x" * 100)
        temp.flush()

        w = CompilerWarning()
        w["filename"] = temp.name
        w["line"] = 1
        w["column"] = 4
        w["message"] = "foo bar"

        # Should not throw.
        db.insert(w)

        w["filename"] = "DOES_NOT_EXIST"

        with self.assertRaises(Exception):
            db.insert(w)

    def test_pruning(self):
        """Ensure old warnings are removed from database appropriately."""
        db = WarningsDatabase()

        source_files = []
        for i in range(1, 21):
            temp = NamedTemporaryFile(mode="wt")
            temp.write("x" * (100 * i))
            temp.flush()

            # Keep reference so it doesn't get GC'd and deleted.
            source_files.append(temp)

            w = CompilerWarning()
            w["filename"] = temp.name
            w["line"] = 1
            w["column"] = i * 10
            w["message"] = "irrelevant"

            db.insert(w)

        self.assertEqual(len(db), 20)

        # If we change a source file, inserting a new warning should nuke the
        # old one.
        source_files[0].write("extra")
        source_files[0].flush()

        w = CompilerWarning()
        w["filename"] = source_files[0].name
        w["line"] = 1
        w["column"] = 50
        w["message"] = "replaced"

        db.insert(w)

        self.assertEqual(len(db), 20)

        warnings = list(db.warnings_for_file(source_files[0].name))
        self.assertEqual(len(warnings), 1)
        self.assertEqual(warnings[0]["column"], w["column"])

        # If we delete the source file, calling prune should cause the warnings
        # to go away.
        old_filename = source_files[0].name
        del source_files[0]

        self.assertFalse(os.path.exists(old_filename))

        db.prune()
        self.assertEqual(len(db), 19)


if __name__ == "__main__":
    main()
