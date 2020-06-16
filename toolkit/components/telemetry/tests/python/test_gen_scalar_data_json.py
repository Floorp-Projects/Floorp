# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import mozunit
import os
import sys
import tempfile
import unittest
from StringIO import StringIO
from os import path

TELEMETRY_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir, path.pardir))
sys.path.append(TELEMETRY_ROOT_PATH)
# The generators live in "build_scripts", account for that.
sys.path.append(path.join(TELEMETRY_ROOT_PATH, "build_scripts"))
import gen_scalar_data   # noqa: E402


class TestScalarDataJson(unittest.TestCase):

    maxDiff = None

    def test_JSON_definitions_generation(self):
        SCALARS_YAML = """
newscalar:
  withoptin:
    bug_numbers:
      - 1456415
    description: opt-in scalar
    expires: never
    kind: uint
    notification_emails: ["telemetry-client-dev@mozilla.org"]
    record_in_processes: ["main"]
    release_channel_collection: opt-in
    products:
      - firefox
    keyed: false
  withoptout:
    bug_numbers:
      - 1456415
    description: opt-out scalar
    expires: never
    kind: string
    notification_emails: ["telemetry-client-dev@mozilla.org"]
    record_in_processes: ["main"]
    release_channel_collection: opt-out
    products: ["firefox", "fennec"]
    keyed: false
        """

        EXPECTED_JSON = {
            "newscalar": {
                "withoptout": {
                    "kind": "nsITelemetry::SCALAR_TYPE_STRING",
                    "expired": False,
                    "expires": "never",
                    "record_on_release": True,
                    "keyed": False,
                    "keys": [],
                    "stores": ["main"],
                    "products": ["firefox", "fennec"],
                },
                "withoptin": {
                    "kind": "nsITelemetry::SCALAR_TYPE_COUNT",
                    "expired": False,
                    "expires": "never",
                    "record_on_release": False,
                    "keyed": False,
                    "keys": [],
                    "stores": ["main"],
                    "products": ["firefox"],
                }
            }
        }

        io = StringIO()
        try:
            tmpfile = tempfile.NamedTemporaryFile(suffix=".json", delete=False)
            # Write the scalar definition to the temporary file
            tmpfile.write(SCALARS_YAML)
            tmpfile.close()

            # Let the parser generate the artifact definitions
            gen_scalar_data.generate_JSON_definitions(io, tmpfile.name)
        finally:
            if tmpfile:
                os.unlink(tmpfile.name)

        scalar_definitions = json.loads(io.getvalue())

        # Check that it generated the correct data
        self.assertEqual(EXPECTED_JSON, scalar_definitions)


if __name__ == '__main__':
    mozunit.main()
