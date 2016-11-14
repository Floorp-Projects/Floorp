# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pytest

from mock import Mock, patch, sentinel

from marionette.runtests import MarionetteTestRunner, MarionetteHarness, cli

import marionette.marionette_test as marionette_test


@pytest.fixture
def harness_class(request):
    """
    Mock based on MarionetteHarness whose run method just returns a number of
    failures according to the supplied test parameter
    """
    if 'num_fails_crashed' in request.funcargnames:
        num_fails_crashed = request.getfuncargvalue('num_fails_crashed')
    else:
        num_fails_crashed = (0, 0)
    harness_cls = Mock(spec=MarionetteHarness)
    harness = harness_cls.return_value
    if num_fails_crashed is None:
        harness.run.side_effect = Exception
    else:
        harness.run.return_value = sum(num_fails_crashed)
    return harness_cls


@pytest.fixture
def runner_class(request):
    """
    Mock based on MarionetteTestRunner, wherein the runner.failed,
    runner.crashed attributes are provided by a test parameter
    """
    if 'num_fails_crashed' in request.funcargnames:
        failures, crashed = request.getfuncargvalue('num_fails_crashed')
    else:
        failures = 0
        crashed = 0
    mock_runner_class = Mock(spec=MarionetteTestRunner)
    runner = mock_runner_class.return_value
    runner.failed = failures
    runner.crashed = crashed
    return mock_runner_class


@pytest.mark.parametrize(
    "num_fails_crashed,exit_code",
    [((0, 0), 0), ((1, 0), 10), ((0, 1), 10), (None, 1)],
)
def test_cli_exit_code(num_fails_crashed, exit_code, harness_class):
    with pytest.raises(SystemExit) as err:
        cli(harness_class=harness_class)
    assert err.value.code == exit_code


@pytest.mark.parametrize("num_fails_crashed", [(0, 0), (1, 0), (1, 1)])
def test_call_harness_with_parsed_args_yields_num_failures(mach_parsed_kwargs,
                                                           runner_class,
                                                           num_fails_crashed):
    with patch(
        'marionette.runtests.MarionetteHarness.parse_args'
    ) as parse_args:
        failed_or_crashed = MarionetteHarness(runner_class,
                                              args=mach_parsed_kwargs).run()
        parse_args.assert_not_called()
    assert failed_or_crashed == sum(num_fails_crashed)


def test_call_harness_with_no_args_yields_num_failures(runner_class):
    with patch(
        'marionette.runtests.MarionetteHarness.parse_args',
        return_value={'tests': []}
    ) as parse_args:
        failed_or_crashed = MarionetteHarness(runner_class).run()
        assert parse_args.call_count == 1
    assert failed_or_crashed == 0


def test_args_passed_to_runner_class(mach_parsed_kwargs, runner_class):
    arg_list = mach_parsed_kwargs.keys()
    arg_list.remove('tests')
    mach_parsed_kwargs.update([(a, getattr(sentinel, a)) for a in arg_list])
    harness = MarionetteHarness(runner_class, args=mach_parsed_kwargs)
    harness.process_args = Mock()
    harness.run()
    for arg in arg_list:
        assert harness._runner_class.call_args[1][arg] is getattr(sentinel, arg)


def test_harness_sets_up_default_test_handlers(mach_parsed_kwargs):
    """
    If the necessary TestCase is not in test_handlers,
    tests are omitted silently
    """
    harness = MarionetteHarness(args=mach_parsed_kwargs)
    mach_parsed_kwargs.pop('tests')
    runner = harness._runner_class(**mach_parsed_kwargs)
    assert marionette_test.MarionetteTestCase in runner.test_handlers


if __name__ == '__main__':
    import sys
    sys.exit(pytest.main(['--verbose', __file__]))
