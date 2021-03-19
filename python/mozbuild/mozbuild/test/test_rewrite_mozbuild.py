# coding: utf-8
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from mozunit import main
import mozbuild.vendor.rewrite_mozbuild as mu


class TestUtils(unittest.TestCase):
    def test_normalize_filename(self):
        self.assertEqual(mu.normalize_filename("foo/bar/moz.build", "/"), "/")
        self.assertEqual(
            mu.normalize_filename("foo/bar/moz.build", "a.c"), "foo/bar/a.c"
        )
        self.assertEqual(
            mu.normalize_filename("foo/bar/moz.build", "baz/a.c"), "foo/bar/baz/a.c"
        )
        self.assertEqual(mu.normalize_filename("foo/bar/moz.build", "/a.c"), "/a.c")

    def test_find_all_posible_assignments_from_filename(self):
        test_vectors = [
            # (
            # target_filename_normalized
            # source_assignments
            # expected
            # )
            (
                "root/dir/asm/blah.S",
                {
                    "> SOURCES": ["root/dir/main.c"],
                    "> if conditional > SOURCES": ["root/dir/asm/blah.S"],
                },
                {"> if conditional > SOURCES": ["root/dir/asm/blah.S"]},
            ),
            (
                "root/dir/dostuff.c",
                {
                    "> SOURCES": ["root/dir/main.c"],
                    "> if conditional > SOURCES": ["root/dir/asm/blah.S"],
                },
                {
                    "> SOURCES": ["root/dir/main.c"],
                },
            ),
        ]

        for vector in test_vectors:
            target_filename_normalized, source_assignments, expected = vector
            actual = mu.find_all_posible_assignments_from_filename(
                source_assignments, target_filename_normalized
            )
            self.assertEqual(actual, expected)

    def test_filenames_directory_is_in_filename_list(self):
        test_vectors = [
            # (
            # normalized filename
            # list of normalized_filenames
            # expected
            # )
            ("foo/bar/a.c", ["foo/b.c"], False),
            ("foo/bar/a.c", ["foo/b.c", "foo/bar/c.c"], True),
            ("foo/bar/a.c", ["foo/b.c", "foo/bar/baz/d.c"], False),
        ]
        for vector in test_vectors:
            normalized_filename, list_of_normalized_filesnames, expected = vector
            actual = mu.filenames_directory_is_in_filename_list(
                normalized_filename, list_of_normalized_filesnames
            )
            self.assertEqual(actual, expected)

    def test_guess_best_assignment(self):
        test_vectors = [
            # (
            # filename_normalized
            # source_assignments
            # expected
            # )
            (
                "foo/asm_arm.c",
                {
                    "> SOURCES": ["foo/main.c", "foo/all_utility.c"],
                    "> if ASM > SOURCES": ["foo/asm_x86.c"],
                },
                "> if ASM > SOURCES",
            ),
        ]
        for vector in test_vectors:
            normalized_filename, source_assignments, expected = vector
            actual, _ = mu.guess_best_assignment(
                source_assignments, normalized_filename
            )
            self.assertEqual(actual, expected)


if __name__ == "__main__":
    main()
