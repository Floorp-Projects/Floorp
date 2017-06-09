# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import sys

import pytest

from conftest import build, filter_action

sys.path.insert(0, os.path.join(build.topsrcdir, 'testing', 'mozharness'))
from mozharness.base.log import INFO, WARNING
from mozharness.base.errors import BaseErrorList
from mozharness.mozilla.buildbot import TBPL_SUCCESS, TBPL_WARNING
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.errors import HarnessErrorList


def get_mozharness_status(lines, status):
    parser = StructuredOutputParser(
        config={'log_level': INFO},
        error_list=BaseErrorList+HarnessErrorList,
        strict=False,
        suite_category='mochitest',
    )

    for line in lines:
        parser.parse_single_line(json.dumps(line))
    return parser.evaluate_parser(status)


def test_output_pass(runtests):
    status, lines = runtests('test_pass.html')
    assert status == 0

    tbpl_status, log_level = get_mozharness_status(lines, status)
    assert tbpl_status == TBPL_SUCCESS
    assert log_level == WARNING

    lines = filter_action('test_status', lines)
    assert len(lines) == 1
    assert lines[0]['status'] == 'PASS'


def test_output_fail(runtests):
    from runtests import build_obj

    status, lines = runtests('test_fail.html')
    assert status == 1

    tbpl_status, log_level = get_mozharness_status(lines, status)
    assert tbpl_status == TBPL_WARNING
    assert log_level == WARNING

    lines = filter_action('test_status', lines)

    # If we are running with a build_obj, the failed status will be
    # logged a second time at the end of the run.
    if build_obj:
        assert len(lines) == 2
    else:
        assert len(lines) == 1
    assert lines[0]['status'] == 'FAIL'

    if build_obj:
        assert set(lines[0].keys()) == set(lines[1].keys())
        assert set(lines[0].values()) == set(lines[1].values())


if __name__ == '__main__':
    sys.exit(pytest.main(['--verbose', __file__]))
