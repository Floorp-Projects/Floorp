#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from mock import patch

import mozunit
import pytest


@pytest.mark.parametrize('logger', [True, False])
def test_crash_count_with_or_without_logger(runner, logger):
    if runner.app == 'chrome':
        pytest.xfail("crash checking not implemented for ChromeRunner")

    if not logger:
        runner.logger = None
        fn = 'check_for_crashes'
    else:
        fn = 'log_crashes'

    with patch('mozcrash.{}'.format(fn), return_value=2) as mock:
        assert runner.crashed == 0
        assert runner.check_for_crashes() == 2
        assert runner.crashed == 2
        assert runner.check_for_crashes() == 2
        assert runner.crashed == 4

        mock.return_value = 0
        assert runner.check_for_crashes() == 0
        assert runner.crashed == 4


if __name__ == '__main__':
    mozunit.main()
