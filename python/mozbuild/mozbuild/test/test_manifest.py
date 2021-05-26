# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

import mozfile
from mozunit import main
from mozbuild.vendor.moz_yaml import load_moz_yaml, MozYamlVerifyError


class TestManifest(unittest.TestCase):
    def process_test_vectors(self, test_vectors):
        index = 0
        for vector in test_vectors:
            print("Testing index", index)
            expected, yaml = vector
            with mozfile.NamedTemporaryFile() as tf:
                tf.write(yaml)
                tf.flush()
                if expected == "exception":
                    with self.assertRaises(MozYamlVerifyError):
                        load_moz_yaml(tf.name, require_license_file=False)
                else:
                    self.assertDictEqual(
                        load_moz_yaml(tf.name, require_license_file=False), expected
                    )
            index += 1

    # ===========================================================================================
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

        self.process_test_vectors(
            [
                (
                    simple_dict,
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
            """.strip(),
                ),
                (
                    simple_dict,
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
            """.strip(),
                ),
            ]
        )

    # ===========================================================================================
    def test_updatebot(self):
        self.process_test_vectors(
            [
                (
                    {
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
                        "updatebot": {
                            "maintainer-phab": "tjr",
                            "maintainer-bz": "a@example.com",
                        },
                    },
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
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
            """.strip(),
                ),
                # -------------------------------------------------
                (
                    {
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
                        "vendoring": {
                            "url": "https://example.com",
                            "source-hosting": "gitlab",
                        },
                        "updatebot": {
                            "maintainer-phab": "tjr",
                            "maintainer-bz": "a@example.com",
                            "tasks": [{"type": "commit-alert"}],
                        },
                    },
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
bugzilla:
  product: Core
  component: Graphics
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: commit-alert
            """.strip(),
                ),
                # -------------------------------------------------
                (
                    {
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
                        "vendoring": {
                            "url": "https://example.com",
                            "source-hosting": "gitlab",
                        },
                        "updatebot": {
                            "maintainer-phab": "tjr",
                            "maintainer-bz": "a@example.com",
                            "tasks": [
                                {
                                    "type": "commit-alert",
                                    "frequency": "release",
                                },
                                {
                                    "type": "vendoring",
                                    "branch": "foo",
                                    "enabled": False,
                                    "cc": ["b@example.com"],
                                    "needinfo": ["c@example.com"],
                                    "frequency": "1 weeks",
                                },
                            ],
                        },
                    },
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
bugzilla:
  product: Core
  component: Graphics
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: commit-alert
      frequency: release
    - type: vendoring
      enabled: False
      branch: foo
      cc: ["b@example.com"]
      needinfo: ["c@example.com"]
      frequency: 1 weeks
            """.strip(),
                ),
                # -------------------------------------------------
                (
                    {
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
                        "vendoring": {
                            "url": "https://example.com",
                            "source-hosting": "gitlab",
                        },
                        "updatebot": {
                            "maintainer-phab": "tjr",
                            "maintainer-bz": "a@example.com",
                            "tasks": [
                                {
                                    "type": "vendoring",
                                    "branch": "foo",
                                    "enabled": False,
                                    "cc": ["b@example.com", "c@example.com"],
                                    "needinfo": ["d@example.com", "e@example.com"],
                                    "frequency": "every",
                                },
                                {
                                    "type": "commit-alert",
                                    "filter": "none",
                                    "source-extensions": [".c", ".cpp"],
                                    "frequency": "2 weeks",
                                },
                            ],
                        },
                    },
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
bugzilla:
  product: Core
  component: Graphics
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: vendoring
      enabled: False
      branch: foo
      cc:
        - b@example.com
        - c@example.com
      needinfo:
        - d@example.com
        - e@example.com
      frequency: every
    - type: commit-alert
      filter: none
      frequency: 2 weeks
      source-extensions:
        - .c
        - .cpp
            """.strip(),
                ),
                # -------------------------------------------------
                (
                    "exception",
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
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: vendoring
      enabled: False
      branch: foo
      cc:
        - b@example.com
        - c@example.com
    - type: commit-alert
      filter: none
      source-extensions:
        - .c
        - .cpp
            """.strip(),
                ),
                # -------------------------------------------------
                (
                    "exception",
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
bugzilla:
  product: Core
  component: Graphics
vendoring:
  url: https://example.com
  source-hosting: gitlab
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: vendoring
      enabled: False
      branch: foo
      cc:
        - b@example.com
        - c@example.com
    - type: commit-alert
      filter: none
      source-extensions:
        - .c
        - .cpp
            """.strip(),
                ),
                # -------------------------------------------------
                (
                    "exception",
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
bugzilla:
  product: Core
  component: Graphics
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: vendoring
      filter: none
            """.strip(),
                ),
                # -------------------------------------------------
                (
                    "exception",
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
bugzilla:
  product: Core
  component: Graphics
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: foo
            """.strip(),
                ),
                # -------------------------------------------------
                (
                    "exception",
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
bugzilla:
  product: Core
  component: Graphics
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: vendoring
      source-extensions:
        - .c
        - .cpp
            """.strip(),
                ),
                # -------------------------------------------------
                (
                    "exception",
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
bugzilla:
  product: Core
  component: Graphics
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: commit-alert
      filter: hogwash
            """.strip(),
                ),
                (
                    "exception",
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: vendoring
      enabled: False
      branch: foo
      cc:
        - b@example.com
        - c@example.com
    - type: commit-alert
    - type: commit-alert
      filter: none
      source-extensions:
        - .c
        - .cpp""".strip(),
                ),
                (
                    "exception",
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: vendoring
      enabled: False
      branch: foo
      cc:
        - b@example.com
        - c@example.com
    - type: vendoring
    - type: commit-alert
      filter: none
      source-extensions:
        - .c
        - .cpp""".strip(),
                ),
                (
                    "exception",
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: vendoring
      enabled: False
      branch: foo
      cc:
        - b@example.com
        - c@example.com
    - type: commit-alert
      frequency: every-release
      filter: none
      source-extensions:
        - .c
        - .cpp""".strip(),
                ),
                (
                    "exception",
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
vendoring:
  url: https://example.com
  source-hosting: gitlab
updatebot:
  maintainer-phab: tjr
  maintainer-bz: a@example.com
  tasks:
    - type: vendoring
      enabled: False
      branch: foo
      cc:
        - b@example.com
        - c@example.com
      frequency: 2 months
    - type: commit-alert
      filter: none
      source-extensions:
        - .c
        - .cpp""".strip(),
                ),
            ]
        )

    # ===========================================================================================
    def test_malformed(self):
        with mozfile.NamedTemporaryFile() as tf:
            tf.write(b"blah")
            tf.flush()
            with self.assertRaises(MozYamlVerifyError):
                load_moz_yaml(tf.name, require_license_file=False)

    def test_schema(self):
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

    def test_revision(self):
        self.process_test_vectors(
            [
                (
                    {
                        "schema": 1,
                        "origin": {
                            "description": "2D Graphics Library",
                            "license": ["MPL-1.1", "LGPL-2.1"],
                            "name": "cairo",
                            "release": "version 1.6.4",
                            "revision": "v1.6.37",
                            "url": "https://www.cairographics.org/",
                        },
                        "bugzilla": {
                            "component": "Graphics",
                            "product": "Core",
                        },
                    },
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
  revision: v1.6.37
bugzilla:
  product: Core
  component: Graphics""".strip(),
                ),
                (
                    "exception",
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
  revision: 4.0.0.
bugzilla:
  product: Core
  component: Graphics""".strip(),
                ),
                (
                    "exception",
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
  revision: 4.^.0
bugzilla:
  product: Core
  component: Graphics""".strip(),
                ),
                (
                    "exception",
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
  revision: " "
bugzilla:
  product: Core
  component: Graphics""".strip(),
                ),
                (
                    "exception",
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
  revision: ???
bugzilla:
  product: Core
  component: Graphics""".strip(),
                ),
                (
                    "exception",
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
  revision: ]
bugzilla:
  product: Core
  component: Graphics""".strip(),
                ),
            ]
        )


if __name__ == "__main__":
    main()
