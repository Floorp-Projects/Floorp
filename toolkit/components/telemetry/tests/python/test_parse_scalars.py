# This Source Code Form is subject to the terms of Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import yaml
import mozunit
import sys
import unittest
from os import path

TELEMETRY_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir, path.pardir))
sys.path.append(TELEMETRY_ROOT_PATH)
from shared_telemetry_utils import ParserError
import parse_scalars


def load_scalar(scalar):
    """Parse the passed Scalar and return a dictionary

    :param scalar: Scalar as YAML string
    :returns: Parsed Scalar dictionary
    """
    return yaml.safe_load(scalar)


class TestParser(unittest.TestCase):
    def test_valid_email_address(self):
        SAMPLE_SCALAR_VALID_ADDRESSES = """
description: A nice one-line description.
expires: never
record_in_processes:
  - 'main'
kind: uint
notification_emails:
  - test01@mozilla.com
  - test02@mozilla.com
bug_numbers:
  - 12345
"""
        scalar = load_scalar(SAMPLE_SCALAR_VALID_ADDRESSES)
        sclr = parse_scalars.ScalarType("CATEGORY",
                                        "PROVE",
                                        scalar,
                                        strict_type_checks=True)

        self.assertEqual(sclr.notification_emails, ["test01@mozilla.com", "test02@mozilla.com"])

    def test_invalid_email_address(self):
        SAMPLE_SCALAR_INVALID_ADDRESSES = """
description: A nice one-line description.
expires: never
  - 'main'
kind: uint
notification_emails:
  - test01@mozilla.com, test02@mozilla.com
bug_numbers:
  - 12345
"""
        scalar = load_scalar(SAMPLE_SCALAR_INVALID_ADDRESSES)
        parse_scalars.ScalarType("CATEGORY",
                                 "PROVE",
                                 scalar,
                                 strict_type_checks=True)

        self.assertRaises(SystemExit, ParserError.exit_func)


if __name__ == '__main__':
    mozunit.main()
