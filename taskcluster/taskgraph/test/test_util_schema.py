# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from six import text_type
import unittest
from mozunit import main
from taskgraph.util.schema import (
    validate_schema,
    resolve_keyed_by,
    Schema,
)

schema = Schema(
    {
        "x": int,
        "y": text_type,
    }
)


class TestValidateSchema(unittest.TestCase):
    def test_valid(self):
        validate_schema(schema, {"x": 10, "y": "foo"}, "pfx")

    def test_invalid(self):
        try:
            validate_schema(schema, {"x": "not-int"}, "pfx")
            self.fail("no exception raised")
        except Exception as e:
            self.assertTrue(str(e).startswith("pfx\n"))


class TestCheckSchema(unittest.TestCase):
    def test_schema(self):
        "Creating a schema applies taskgraph checks."
        with self.assertRaises(Exception):
            Schema({"camelCase": int})

    def test_extend_schema(self):
        "Extending a schema applies taskgraph checks."
        with self.assertRaises(Exception):
            Schema({"kebab-case": int}).extend({"camelCase": int})

    def test_extend_schema_twice(self):
        "Extending a schema twice applies taskgraph checks."
        with self.assertRaises(Exception):
            Schema({"kebab-case": int}).extend({"more-kebab": int}).extend(
                {"camelCase": int}
            )


class TestResolveKeyedBy(unittest.TestCase):
    def test_no_by(self):
        self.assertEqual(resolve_keyed_by({"x": 10}, "z", "n"), {"x": 10})

    def test_no_by_dotted(self):
        self.assertEqual(
            resolve_keyed_by({"x": {"y": 10}}, "x.z", "n"), {"x": {"y": 10}}
        )

    def test_no_by_not_dict(self):
        self.assertEqual(resolve_keyed_by({"x": 10}, "x.y", "n"), {"x": 10})

    def test_no_by_not_by(self):
        self.assertEqual(resolve_keyed_by({"x": {"a": 10}}, "x", "n"), {"x": {"a": 10}})

    def test_nested(self):
        x = {
            "by-foo": {
                "F1": {
                    "by-bar": {
                        "B1": 11,
                        "B2": 12,
                    },
                },
                "F2": 20,
                "default": 0,
            },
        }
        self.assertEqual(
            resolve_keyed_by({"x": x}, "x", "x", foo="F1", bar="B1"), {"x": 11}
        )
        self.assertEqual(
            resolve_keyed_by({"x": x}, "x", "x", foo="F1", bar="B2"), {"x": 12}
        )
        self.assertEqual(resolve_keyed_by({"x": x}, "x", "x", foo="F2"), {"x": 20})
        self.assertEqual(
            resolve_keyed_by({"x": x}, "x", "x", foo="F99", bar="B1"), {"x": 0}
        )

        # bar is deferred
        self.assertEqual(
            resolve_keyed_by({"x": x}, "x", "x", defer=["bar"], foo="F1", bar="B1"),
            {"x": {"by-bar": {"B1": 11, "B2": 12}}},
        )

    def test_no_by_empty_dict(self):
        self.assertEqual(resolve_keyed_by({"x": {}}, "x", "n"), {"x": {}})

    def test_no_by_not_only_by(self):
        self.assertEqual(
            resolve_keyed_by({"x": {"by-y": True, "a": 10}}, "x", "n"),
            {"x": {"by-y": True, "a": 10}},
        )

    def test_match_nested_exact(self):
        self.assertEqual(
            resolve_keyed_by(
                {
                    "f": "shoes",
                    "x": {"y": {"by-f": {"shoes": "feet", "gloves": "hands"}}},
                },
                "x.y",
                "n",
            ),
            {"f": "shoes", "x": {"y": "feet"}},
        )

    def test_match_regexp(self):
        self.assertEqual(
            resolve_keyed_by(
                {
                    "f": "shoes",
                    "x": {"by-f": {"s?[hH]oes?": "feet", "gloves": "hands"}},
                },
                "x",
                "n",
            ),
            {"f": "shoes", "x": "feet"},
        )

    def test_match_partial_regexp(self):
        self.assertEqual(
            resolve_keyed_by(
                {"f": "shoes", "x": {"by-f": {"sh": "feet", "default": "hands"}}},
                "x",
                "n",
            ),
            {"f": "shoes", "x": "hands"},
        )

    def test_match_default(self):
        self.assertEqual(
            resolve_keyed_by(
                {"f": "shoes", "x": {"by-f": {"hat": "head", "default": "anywhere"}}},
                "x",
                "n",
            ),
            {"f": "shoes", "x": "anywhere"},
        )

    def test_match_extra_value(self):
        self.assertEqual(
            resolve_keyed_by({"f": {"by-foo": {"x": 10, "y": 20}}}, "f", "n", foo="y"),
            {"f": 20},
        )

    def test_no_match(self):
        self.assertRaises(
            Exception,
            resolve_keyed_by,
            {"f": "shoes", "x": {"by-f": {"hat": "head"}}},
            "x",
            "n",
        )

    def test_multiple_matches(self):
        self.assertRaises(
            Exception,
            resolve_keyed_by,
            {"f": "hats", "x": {"by-f": {"hat.*": "head", "ha.*": "hair"}}},
            "x",
            "n",
        )

        self.assertEqual(
            resolve_keyed_by(
                {"f": "hats", "x": {"by-f": {"hat.*": "head", "ha.*": "hair"}}},
                "x",
                "n",
                enforce_single_match=False,
            ),
            {"f": "hats", "x": "head"},
        )

    def test_no_key_no_default(self):
        """
        When the key referenced in `by-*` doesn't exist, and there is not default value,
        an exception is raised.
        """
        self.assertRaises(
            Exception,
            resolve_keyed_by,
            {"x": {"by-f": {"hat.*": "head", "ha.*": "hair"}}},
            "x",
            "n",
        )

    def test_no_key(self):
        """
        When the key referenced in `by-*` doesn't exist, and there is a default value,
        that value is used as the result.
        """
        self.assertEqual(
            resolve_keyed_by(
                {"x": {"by-f": {"hat": "head", "default": "anywhere"}}},
                "x",
                "n",
            ),
            {"x": "anywhere"},
        )


if __name__ == "__main__":
    main()
