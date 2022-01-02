# -*- coding: utf-8 -*-

from __future__ import absolute_import, print_function, unicode_literals

import os
import unittest

from six import StringIO

import mozpack.path as mozpath

from mozbuild.dotproperties import DotProperties

from mozunit import main

test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, "data")


class TestDotProperties(unittest.TestCase):
    def test_get(self):
        contents = StringIO(
            """
key=value
"""
        )
        p = DotProperties(contents)
        self.assertEqual(p.get("missing"), None)
        self.assertEqual(p.get("missing", "default"), "default")
        self.assertEqual(p.get("key"), "value")

    def test_update(self):
        contents = StringIO(
            """
old=old value
key=value
"""
        )
        p = DotProperties(contents)
        self.assertEqual(p.get("old"), "old value")
        self.assertEqual(p.get("key"), "value")

        new_contents = StringIO(
            """
key=new value
"""
        )
        p.update(new_contents)
        self.assertEqual(p.get("old"), "old value")
        self.assertEqual(p.get("key"), "new value")

    def test_get_list(self):
        contents = StringIO(
            """
list.0=A
list.1=B
list.2=C

order.1=B
order.0=A
order.2=C
"""
        )
        p = DotProperties(contents)
        self.assertEqual(p.get_list("missing"), [])
        self.assertEqual(p.get_list("list"), ["A", "B", "C"])
        self.assertEqual(p.get_list("order"), ["A", "B", "C"])

    def test_get_list_with_shared_prefix(self):
        contents = StringIO(
            """
list.0=A
list.1=B
list.2=C

list.sublist.1=E
list.sublist.0=D
list.sublist.2=F

list.sublist.second.0=G

list.other.0=H
"""
        )
        p = DotProperties(contents)
        self.assertEqual(p.get_list("list"), ["A", "B", "C"])
        self.assertEqual(p.get_list("list.sublist"), ["D", "E", "F"])
        self.assertEqual(p.get_list("list.sublist.second"), ["G"])
        self.assertEqual(p.get_list("list.other"), ["H"])

    def test_get_dict(self):
        contents = StringIO(
            """
A.title=title A

B.title=title B
B.url=url B

C=value
"""
        )
        p = DotProperties(contents)
        self.assertEqual(p.get_dict("missing"), {})
        self.assertEqual(p.get_dict("A"), {"title": "title A"})
        self.assertEqual(p.get_dict("B"), {"title": "title B", "url": "url B"})
        with self.assertRaises(ValueError):
            p.get_dict("A", required_keys=["title", "url"])
        with self.assertRaises(ValueError):
            p.get_dict("missing", required_keys=["key"])
        # A key=value pair is considered to root an empty dict.
        self.assertEqual(p.get_dict("C"), {})
        with self.assertRaises(ValueError):
            p.get_dict("C", required_keys=["missing_key"])

    def test_get_dict_with_shared_prefix(self):
        contents = StringIO(
            """
A.title=title A
A.subdict.title=title A subdict

B.title=title B
B.url=url B
B.subdict.title=title B subdict
B.subdict.url=url B subdict
"""
        )
        p = DotProperties(contents)
        self.assertEqual(p.get_dict("A"), {"title": "title A"})
        self.assertEqual(p.get_dict("B"), {"title": "title B", "url": "url B"})
        self.assertEqual(p.get_dict("A.subdict"), {"title": "title A subdict"})
        self.assertEqual(
            p.get_dict("B.subdict"),
            {"title": "title B subdict", "url": "url B subdict"},
        )

    def test_get_dict_with_value_prefix(self):
        contents = StringIO(
            """
A.default=A
A.default.B=B
A.default.B.ignored=B ignored
A.default.C=C
A.default.C.ignored=C ignored
"""
        )
        p = DotProperties(contents)
        self.assertEqual(p.get("A.default"), "A")
        # This enumerates the properties.
        self.assertEqual(p.get_dict("A.default"), {"B": "B", "C": "C"})
        # They can still be fetched directly.
        self.assertEqual(p.get("A.default.B"), "B")
        self.assertEqual(p.get("A.default.C"), "C")

    def test_unicode(self):
        contents = StringIO(
            """
# Danish.
# ####  ~~ Søren Munk Skrøder, sskroeder - 2009-05-30 @ #mozmae

# Korean.
A.title=한메일

# Russian.
list.0 = test
list.1 = Яндекс
"""
        )
        p = DotProperties(contents)
        self.assertEqual(p.get_dict("A"), {"title": "한메일"})
        self.assertEqual(p.get_list("list"), ["test", "Яндекс"])

    def test_valid_unicode_from_file(self):
        # The contents of valid.properties is identical to the contents of the
        # test above.  This specifically exercises reading from a file.
        p = DotProperties(os.path.join(test_data_path, "valid.properties"))
        self.assertEqual(p.get_dict("A"), {"title": "한메일"})
        self.assertEqual(p.get_list("list"), ["test", "Яндекс"])

    def test_bad_unicode_from_file(self):
        # The contents of bad.properties is not valid Unicode; see the comments
        # in the file itself for details.
        with self.assertRaises(UnicodeDecodeError):
            DotProperties(os.path.join(test_data_path, "bad.properties"))


if __name__ == "__main__":
    main()
