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
# NOTE: if the generators are moved, this logic will need to be updated.
sys.path.append(path.join(TELEMETRY_ROOT_PATH, "build_scripts"))
import gen_event_data   # noqa: E402


class TestEventDataJson(unittest.TestCase):

    maxDiff = None

    def test_JSON_definitions_generation(self):
        EVENTS_YAML = """
with.optout:
  testme1:
    objects: ["test1"]
    bug_numbers: [1456415]
    notification_emails: ["telemetry-client-dev@mozilla.org"]
    record_in_processes: ["main"]
    description: opt-out event
    release_channel_collection: opt-out
    expiry_version: never
    products:
      - firefox
    extra_keys:
      message: a message 1
with.optin:
  testme2:
    objects: ["test2"]
    bug_numbers: [1456415]
    notification_emails: ["telemetry-client-dev@mozilla.org"]
    record_in_processes: ["main"]
    description: opt-in event
    release_channel_collection: opt-in
    expiry_version: never
    products: ['firefox', 'fennec']
    extra_keys:
      message: a message 2
        """

        EXPECTED_JSON = {
            "with.optout": {
                "testme1": {
                    "objects": ["test1"],
                    "expired": False,
                    "expires": "never",
                    "methods": ["testme1"],
                    "extra_keys": ["message"],
                    "record_on_release": True,
                    "products": ["firefox"],
                }
            },
            "with.optin": {
                "testme2": {
                    "objects": ["test2"],
                    "expired": False,
                    "expires": "never",
                    "methods": ["testme2"],
                    "extra_keys": ["message"],
                    "record_on_release": False,
                    "products": ["firefox", "fennec"],
                }
            },
        }

        io = StringIO()
        try:
            tmpfile = tempfile.NamedTemporaryFile(suffix=".json", delete=False)
            # Write the event definition to the temporary file
            tmpfile.write(EVENTS_YAML)
            tmpfile.close()

            # Let the parser generate the artifact definitions
            gen_event_data.generate_JSON_definitions(io, tmpfile.name)
        finally:
            if tmpfile:
                os.unlink(tmpfile.name)

        event_definitions = json.loads(io.getvalue())

        # Check that it generated the correct data
        self.assertEqual(EXPECTED_JSON, event_definitions)


if __name__ == '__main__':
    mozunit.main()
