# This Source Code Form is subject to the terms of Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import yaml
import mozunit
import sys
import unittest
import os
from os import path

TELEMETRY_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir, path.pardir))
sys.path.append(TELEMETRY_ROOT_PATH)
# The parsers live in a subdirectory of "build_scripts", account for that.
# NOTE: if the parsers are moved, this logic will need to be updated.
sys.path.append(path.join(TELEMETRY_ROOT_PATH, "build_scripts"))
from mozparsers.shared_telemetry_utils import ParserError
from mozparsers import parse_events


def load_event(event):
    """Parse the passed event and return a dictionary

    :param event: Event as YAML string
    :returns: Parsed Event dictionary
    """
    return yaml.safe_load(event)


class TestParser(unittest.TestCase):
    def setUp(self):
        def mockexit(x):
            raise SystemExit(x)
        self.oldexit = os._exit
        os._exit = mockexit

    def tearDown(self):
        os._exit = self.oldexit

    def test_valid_event_defaults(self):
        SAMPLE_EVENT = """
objects: ["object1", "object2"]
bug_numbers: [12345]
notification_emails: ["test01@mozilla.com", "test02@mozilla.com"]
record_in_processes: ["main"]
description: This is a test entry for Telemetry.
products: ["firefox"]
expiry_version: never
"""
        name = "test_event"
        event = load_event(SAMPLE_EVENT)
        evt = parse_events.EventData("CATEGORY",
                                     name,
                                     event,
                                     strict_type_checks=True)
        ParserError.exit_func()

        self.assertEqual(evt.methods, [name])
        self.assertEqual(evt.record_in_processes, ["main"])
        self.assertEqual(evt.objects, ["object1", "object2"])
        self.assertEqual(evt.products, ["firefox"])
        self.assertEqual(evt.operating_systems, ["all"])
        self.assertEqual(evt.extra_keys, [])

    def test_wrong_collection(self):
        SAMPLE_EVENT = """
objects: ["object1", "object2"]
bug_numbers: [12345]
notification_emails: ["test01@mozilla.com", "test02@mozilla.com"]
record_in_processes: ["main"]
description: This is a test entry for Telemetry.
expiry_version: never
products: ["firefox"]
release_channel_collection: none
"""
        event = load_event(SAMPLE_EVENT)
        parse_events.EventData("CATEGORY",
                               "test_event",
                               event,
                               strict_type_checks=True)

        self.assertRaises(SystemExit, ParserError.exit_func)

    def test_valid_event_custom(self):
        SAMPLE_EVENT = """
methods: ["method1", "method2"]
objects: ["object1", "object2"]
bug_numbers: [12345]
notification_emails: ["test01@mozilla.com", "test02@mozilla.com"]
record_in_processes: ["content"]
description: This is a test entry for Telemetry.
expiry_version: never
extra_keys:
  key1: test1
  key2: test2
products:
    - geckoview
operating_systems:
    - windows
"""
        name = "test_event"
        event = load_event(SAMPLE_EVENT)
        evt = parse_events.EventData("CATEGORY",
                                     name,
                                     event,
                                     strict_type_checks=True)
        ParserError.exit_func()

        self.assertEqual(evt.methods, ["method1", "method2"])
        self.assertEqual(evt.objects, ["object1", "object2"])
        self.assertEqual(evt.record_in_processes, ["content"])
        self.assertEqual(evt.products, ["geckoview"])
        self.assertEqual(evt.operating_systems, ["windows"])
        self.assertEqual(sorted(evt.extra_keys), ["key1", "key2"])

    def test_absent_products(self):
        SAMPLE_EVENT = """
methods: ["method1", "method2"]
objects: ["object1", "object2"]
bug_numbers: [12345]
notification_emails: ["test01@mozilla.com", "test02@mozilla.com"]
record_in_processes: ["content"]
description: This is a test entry for Telemetry.
expiry_version: never
"""
        event = load_event(SAMPLE_EVENT)
        self.assertRaises(SystemExit, lambda: parse_events.EventData("CATEGORY",
                                                                     "test_event",
                                                                     event,
                                                                     strict_type_checks=True))

    def test_empty_products(self):
        SAMPLE_EVENT = """
methods: ["method1", "method2"]
objects: ["object1", "object2"]
bug_numbers: [12345]
notification_emails: ["test01@mozilla.com", "test02@mozilla.com"]
record_in_processes: ["content"]
description: This is a test entry for Telemetry.
products: []
expiry_version: never
"""
        event = load_event(SAMPLE_EVENT)
        self.assertRaises(SystemExit, lambda: parse_events.EventData("CATEGORY",
                                                                     "test_event",
                                                                     event,
                                                                     strict_type_checks=True))

    def test_geckoview_streaming_product(self):
        SAMPLE_EVENT = """
methods: ["method1", "method2"]
objects: ["object1", "object2"]
bug_numbers: [12345]
notification_emails: ["test01@mozilla.com", "test02@mozilla.com"]
record_in_processes: ["content"]
description: This is a test entry for Telemetry.
products: ["geckoview_streaming"]
expiry_version: never
"""
        event = load_event(SAMPLE_EVENT)
        parse_events.EventData("CATEGORY",
                               "test_event",
                               event,
                               strict_type_checks=True)

        self.assertRaises(SystemExit, ParserError.exit_func)


if __name__ == '__main__':
    mozunit.main()
