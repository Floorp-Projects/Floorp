# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from mozlog import structured
from wptrunner.testloader import TestFilter as Filter
from .test_chunker import make_mock_manifest

structured.set_default_logger(structured.structuredlog.StructuredLogger("TestLoader"))

include_ini = """\
skip: true
[test_\u53F0]
  skip: false
"""

def test_filter_unicode():
    tests = make_mock_manifest(("a", 10), ("a/b", 10), ("c", 10))

    with tempfile.NamedTemporaryFile("wb", suffix=".ini") as f:
        f.write(include_ini.encode('utf-8'))
        f.flush()

        Filter(manifest_path=f.name, test_manifests=tests)
