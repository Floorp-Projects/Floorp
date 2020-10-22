# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Methods for testing interactions with mozharness."""

from __future__ import absolute_import

import json
import os
import sys

from mozbuild.base import MozbuildObject
from six import string_types

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)

sys.path.insert(0, os.path.join(build.topsrcdir, 'testing', 'mozharness'))
from mozharness.base.log import INFO
from mozharness.base.errors import BaseErrorList
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.errors import HarnessErrorList


def get_mozharness_status(suite, lines, status, formatter=None, buf=None):
    """Given list of log lines, determine what the mozharness status would be."""
    parser = StructuredOutputParser(
        config={'log_level': INFO},
        error_list=BaseErrorList+HarnessErrorList,
        strict=False,
        suite_category=suite,
    )

    if formatter:
        parser.formatter = formatter

    # Processing the log with mozharness will re-print all the output to stdout
    # Since this exact same output has already been printed by the actual test
    # run, temporarily redirect stdout to devnull.
    buf = buf or open(os.devnull, 'w')
    orig = sys.stdout
    sys.stdout = buf
    for line in lines:
        parser.parse_single_line(json.dumps(line))
    sys.stdout = orig
    return parser.evaluate_parser(status)


def filter_action(actions, lines):
    if isinstance(actions, string_types):
        actions = (actions,)
    return filter(lambda x: x['action'] in actions, lines)
