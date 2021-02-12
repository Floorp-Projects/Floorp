# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

import mozfile
from mozunit import main
from mozbuild.vendor.moz_yaml import load_moz_yaml, MozYamlVerifyError


class TestManifest(unittest.TestCase):
    def test_simple(self):
        simple_dict = {
            "schema": 1,
            "origin": {
                "description": "2D Graphics Library",
                "license": ["MPL-1.1", "LGPL-2.1"],
                "name": "cairo",
                "release": "version 1.6.4",
                "revision": "AA001122334455",
                "url": "https://www.cairographics.org/",
            },
            "bugzilla": {
                "component": "Graphics",
                "product": "Core",
            },
        }
        with mozfile.NamedTemporaryFile() as tf:
            tf.write(
                b"""
---
schema: 1
origin:
  name: cairo
  description: 2D Graphics Library
  url: https://www.cairographics.org/
  release: version 1.6.4
  license:
    - MPL-1.1
    - LGPL-2.1
  revision: AA001122334455
bugzilla:
  product: Core
  component: Graphics
            """.strip()
            )
            tf.flush()
            self.assertDictEqual(
                load_moz_yaml(tf.name, require_license_file=False), simple_dict
            )

        # as above, without the --- yaml prefix
        with mozfile.NamedTemporaryFile() as tf:
            tf.write(
                b"""
schema: 1
origin:
  name: cairo
  description: 2D Graphics Library
  url: https://www.cairographics.org/
  release: version 1.6.4
  license:
    - MPL-1.1
    - LGPL-2.1
  revision: AA001122334455
bugzilla:
  product: Core
  component: Graphics
            """.strip()
            )
            tf.flush()
            self.assertDictEqual(
                load_moz_yaml(tf.name, require_license_file=False), simple_dict
            )

    def test_malformed(self):
        with mozfile.NamedTemporaryFile() as tf:
            tf.write(b"blah")
            tf.flush()
            with self.assertRaises(MozYamlVerifyError):
                load_moz_yaml(tf.name, require_license_file=False)

    def test_bad_schema(self):
        with mozfile.NamedTemporaryFile() as tf:
            tf.write(b"schema: 99")
            tf.flush()
            with self.assertRaises(MozYamlVerifyError):
                load_moz_yaml(tf.name, require_license_file=False)

    def test_json(self):
        with mozfile.NamedTemporaryFile() as tf:
            tf.write(
                b'{"origin": {"release": "version 1.6.4", "url": "https://w'
                b'ww.cairographics.org/", "description": "2D Graphics Libra'
                b'ry", "license": ["MPL-1.1", "LGPL-2.1"], "name": "cairo"}'
                b', "bugzilla": {"product": "Core", "component": "Graphics"'
                b'}, "schema": 1}'
            )
            tf.flush()
            with self.assertRaises(MozYamlVerifyError):
                load_moz_yaml(tf.name, require_license_file=False)


if __name__ == "__main__":
    main()
