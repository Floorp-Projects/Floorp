# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pytest

from marionette.runner import MarionetteTestResult


@pytest.fixture
def empty_marionette_testcase():
    """ Testable MarionetteTestCase class """
    from marionette.marionette_test import MarionetteTestCase

    class EmptyTestCase(MarionetteTestCase):
        def test_nothing(self):
            pass

    return EmptyTestCase


@pytest.fixture
def empty_marionette_test(mock_marionette, empty_marionette_testcase):
    return empty_marionette_testcase(lambda: mock_marionette, lambda: mock_httpd,
                                     'test_nothing')


@pytest.mark.parametrize("has_crashed", [True, False])
def test_crash_is_recorded_as_error(empty_marionette_test,
                                    logger,
                                    has_crashed):
    """ Number of errors is incremented by stopTest iff has_crashed is true """
    # collect results from the empty test
    result = MarionetteTestResult(
        marionette=empty_marionette_test._marionette_weakref(),
        logger=logger, verbosity=None,
        stream=None, descriptions=None,
    )
    result.startTest(empty_marionette_test)
    assert len(result.errors) == 0
    assert len(result.failures) == 0
    assert result.testsRun == 1
    assert result.shouldStop is False
    result.stopTest(empty_marionette_test)
    assert result.shouldStop == has_crashed
    if has_crashed:
        assert len(result.errors) == 1
    else:
        assert len(result.errors) == 0


if __name__ == '__main__':
    import sys
    sys.exit(pytest.main(['--verbose', __file__]))
