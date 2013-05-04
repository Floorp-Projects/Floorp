# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import unicode_literals

import unittest

from mozunit import (
    main,
)

from dumbmake.dumbmake import (
    add_extra_dependencies,
    all_dependencies,
    dependency_map,
    indentation,
)

class TestDumbmake(unittest.TestCase):
    def test_indentation(self):
        self.assertEqual(indentation(""), 0)
        self.assertEqual(indentation("x"), 0)
        self.assertEqual(indentation(" x"), 1)
        self.assertEqual(indentation("\tx"), 1)
        self.assertEqual(indentation(" \tx"), 2)
        self.assertEqual(indentation("\t x"), 2)
        self.assertEqual(indentation(" x  "), 1)
        self.assertEqual(indentation("\tx\t"), 1)
        self.assertEqual(indentation("  x"), 2)
        self.assertEqual(indentation("    x"), 4)

    def test_dependency_map(self):
        self.assertEqual(dependency_map([]), {})
        self.assertEqual(dependency_map(["a"]), {"a": []})
        self.assertEqual(dependency_map(["a", "b"]), {"a": [], "b": []})
        self.assertEqual(dependency_map(["a", "b", "c"]), {"a": [], "b": [], "c": []})
        # indentation
        self.assertEqual(dependency_map(["a", "\tb", "a", "\tc"]), {"a": [], "b": ["a"], "c": ["a"]})
        self.assertEqual(dependency_map(["a", "\tb", "\t\tc"]), {"a": [], "b": ["a"], "c": ["b", "a"]})
        self.assertEqual(dependency_map(["a", "\tb", "\t\tc", "\td", "\te", "f"]), {"a": [], "b": ["a"], "c": ["b", "a"], "d": ["a"], "e": ["a"], "f": []})
        # irregular indentation
        self.assertEqual(dependency_map(["\ta", "b"]), {"a": [], "b": []})
        self.assertEqual(dependency_map(["a", "\t\t\tb", "\t\tc"]), {"a": [], "b": ["a"], "c": ["a"]})
        self.assertEqual(dependency_map(["a", "\t\tb", "\t\t\tc", "\t\td", "\te", "f"]), {"a": [], "b": ["a"], "c": ["b", "a"], "d": ["a"], "e": ["a"], "f": []})
        # repetitions
        self.assertEqual(dependency_map(["a", "\tb", "a", "\tb"]), {"a": [], "b": ["a"]})
        self.assertEqual(dependency_map(["a", "\tb", "\t\tc", "b", "\td", "\t\te"]), {"a": [], "b": ["a"], "d": ["b"], "e": ["d", "b"], "c": ["b", "a"]})
        # cycles are okay
        self.assertEqual(dependency_map(["a", "\tb", "\t\ta"]), {"a": ["b", "a"], "b": ["a"]})

    def test_all_dependencies(self):
        dm = {"a": [], "b": ["a"], "c": ["b", "a"], "d": ["a"], "e": ["a"], "f": []}
        self.assertEqual(all_dependencies("a", dependency_map=dm), ["a"])
        self.assertEqual(all_dependencies("b", dependency_map=dm), ["b", "a"])
        self.assertEqual(all_dependencies("c", "a", "b", dependency_map=dm), ["c", "b", "a"])
        self.assertEqual(all_dependencies("d", dependency_map=dm), ["d", "a"])
        self.assertEqual(all_dependencies("d", "f", "c", dependency_map=dm), ["d", "f", "c", "b", "a"])
        self.assertEqual(all_dependencies("a", "b", dependency_map=dm), ["b", "a"])
        self.assertEqual(all_dependencies("b", "b", dependency_map=dm), ["b", "a"])

    def test_missing_entry(self):
        # a depends on b, which is missing
        dm = {"a": ["b"]}
        self.assertEqual(all_dependencies("a", dependency_map=dm), ["a", "b"])
        self.assertEqual(all_dependencies("a", "b", dependency_map=dm), ["a", "b"])
        self.assertEqual(all_dependencies("b", dependency_map=dm), ["b"])

    def test_two_dependencies(self):
        dm = {"a": ["c"], "b": ["c"], "c": []}
        # suppose a and b both depend on c.  Then we want to build a and b before c...
        self.assertEqual(all_dependencies("a", "b", dependency_map=dm), ["a", "b", "c"])
        # ... but relative order is preserved.
        self.assertEqual(all_dependencies("b", "a", dependency_map=dm), ["b", "a", "c"])

    def test_nested_dependencies(self):
        # a depends on b depends on c depends on d
        dm = {"a": ["b", "c", "d"], "b": ["c", "d"], "c": ["d"]}
        self.assertEqual(all_dependencies("b", "a", dependency_map=dm), ["a", "b", "c", "d"])
        self.assertEqual(all_dependencies("c", "a", dependency_map=dm), ["a", "b", "c", "d"])

    def test_add_extra_dependencies(self):
        # a depends on b depends on c depends on d
        dm = {"a": ["b", "c", "d"], "b": ["c", "d"], "c": ["d"]}
        # Edge cases.
        self.assertEqual(list(add_extra_dependencies([], dependency_map=dm)),
                         [])
        self.assertEqual(list(add_extra_dependencies([(None, "package")], dependency_map=dm)),
                         [(None, "package")])
        # Easy expansion.
        self.assertEqual(list(add_extra_dependencies([("b", None)], dependency_map=dm)),
                         [("b", None), ("c", None), ("d", None)])
        # Expansion with two groups -- each group is handled independently.
        self.assertEqual(list(add_extra_dependencies([("b", None),
                                                      (None, "package"),
                                                      ("c", None)], dependency_map=dm)),
                         [("b", None), ("c", None), ("d", None),
                          (None, "package"),
                          ("c", None), ("d", None)])
        # Two groups, no duplicate dependencies in each group.
        self.assertEqual(list(add_extra_dependencies([("a", None), ("b", None),
                                                      (None, "package"), (None, "install"),
                                                      ("c", None), ("d", None)], dependency_map=dm)),
                         [("a", None), ("b", None), ("c", None), ("d", None),
                          (None, "package"), (None, "install"),
                          ("c", None), ("d", None)])

if __name__ == '__main__':
    main()
