# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import sys

import pytest

from conftest import build, filter_action

sys.path.insert(0, os.path.join(build.topsrcdir, 'testing', 'mozharness'))
from mozharness.base.log import INFO, WARNING, ERROR
from mozharness.base.errors import BaseErrorList
from mozharness.mozilla.buildbot import TBPL_SUCCESS, TBPL_WARNING, TBPL_FAILURE
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.errors import HarnessErrorList

here = os.path.abspath(os.path.dirname(__file__))


def get_mozharness_status(lines, status):
    parser = StructuredOutputParser(
        config={'log_level': INFO},
        error_list=BaseErrorList+HarnessErrorList,
        strict=False,
        suite_category='mochitest',
    )

    # Processing the log with mozharness will re-print all the output to stdout
    # Since this exact same output has already been printed by the actual test
    # run, temporarily redirect stdout to devnull.
    with open(os.devnull, 'w') as fh:
        orig = sys.stdout
        sys.stdout = fh
        for line in lines:
            parser.parse_single_line(json.dumps(line))
        sys.stdout = orig
    return parser.evaluate_parser(status)


def test_output_pass(runtests):
    status, lines = runtests('test_pass.html')
    assert status == 0

    tbpl_status, log_level = get_mozharness_status(lines, status)
    assert tbpl_status == TBPL_SUCCESS
    assert log_level in (INFO, WARNING)

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


def test_output_crash(runtests):
    status, lines = runtests('test_crash.html', environment=["MOZ_CRASHREPORTER_SHUTDOWN=1"])
    assert status == 1

    tbpl_status, log_level = get_mozharness_status(lines, status)
    assert tbpl_status == TBPL_FAILURE
    assert log_level == ERROR

    crash = filter_action('crash', lines)
    assert len(crash) == 1
    assert crash[0]['action'] == 'crash'
    assert crash[0]['signature']
    assert crash[0]['minidump_path']

    lines = filter_action('test_end', lines)
    assert len(lines) == 0


@pytest.mark.skip_mozinfo("!debug")
def test_output_assertion(runtests):
    status, lines = runtests('test_assertion.html')
    # TODO: mochitest should return non-zero here
    assert status == 0

    tbpl_status, log_level = get_mozharness_status(lines, status)
    assert tbpl_status == TBPL_WARNING
    assert log_level == WARNING

    test_end = filter_action('test_end', lines)
    assert len(test_end) == 1
    # TODO: this should be ASSERT, but moving the assertion check before
    # the test_end action caused a bunch of failures.
    assert test_end[0]['status'] == 'OK'

    assertions = filter_action('assertion_count', lines)
    assert len(assertions) == 1
    assert assertions[0]['count'] == 1


@pytest.mark.skip_mozinfo("!debug")
def test_output_leak(monkeypatch, runtests):
    # Monkeypatch mozleak so we always process a failing leak log
    # instead of the actual one.
    import mozleak
    old_process_leak_log = mozleak.process_leak_log

    def process_leak_log(*args, **kwargs):
        return old_process_leak_log(os.path.join(here, 'files', 'leaks.log'), *args[1:], **kwargs)

    monkeypatch.setattr('mozleak.process_leak_log', process_leak_log)

    status, lines = runtests('test_pass.html')
    # TODO: mochitest should return non-zero here
    assert status == 0

    tbpl_status, log_level = get_mozharness_status(lines, status)
    assert tbpl_status == TBPL_FAILURE
    assert log_level == ERROR

    errors = filter_action('log', lines)
    errors = [e for e in errors if e['level'] == 'ERROR']
    assert len(errors) == 1
    assert 'leakcheck' in errors[0]['message']


if __name__ == '__main__':
    sys.exit(pytest.main(['--verbose', __file__]))
