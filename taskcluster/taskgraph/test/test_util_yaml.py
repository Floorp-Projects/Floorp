# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
from textwrap import dedent

from taskgraph.util import yaml
from mozunit import main, MockedOpen


class TestYaml(unittest.TestCase):
    def test_load(self):
        with MockedOpen(
            {
                "/dir1/dir2/foo.yml": dedent(
                    """\
                    prop:
                        - val1
                    """
                )
            }
        ):

            self.assertEqual(
                yaml.load_yaml("/dir1/dir2", "foo.yml"), {"prop": ["val1"]}
            )

    def test_key_order(self):
        with MockedOpen(
            {
                "/dir1/dir2/foo.yml": dedent(
                    """\
                    job:
                        foo: 1
                        bar: 2
                        xyz: 3
                    """
                )
            }
        ):
            self.assertEqual(
                list(yaml.load_yaml("/dir1/dir2", "foo.yml")["job"].keys()),
                ["foo", "bar", "xyz"],
            )


if __name__ == "__main__":
    main()
