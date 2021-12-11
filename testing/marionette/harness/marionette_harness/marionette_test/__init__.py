# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

__version__ = "3.1.0"

from unittest.case import (
    skip,
    SkipTest,
)

from .decorators import (
    parameterized,
    run_if_manage_instance,
    skip_if_chrome,
    skip_if_desktop,
    skip_unless_browser_pref,
    skip_unless_protocol,
    with_parameters,
)

from .testcases import (
    CommonTestCase,
    expectedFailure,
    MarionetteTestCase,
    MetaParameterized,
    unexpectedSuccess,
)
