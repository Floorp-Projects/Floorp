# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

__version__ = '3.1.0'


from unittest.case import (
    expectedFailure,
    skip,
    SkipTest,
)

from .decorators import (
    parameterized,
    run_if_e10s,
    skip_if_chrome,
    skip_if_desktop,
    skip_if_e10s,
    skip_if_mobile,
    skip_unless_browser_pref,
    skip_unless_protocol,
    with_parameters,
)

from .testcases import (
    CommonTestCase,
    MarionetteTestCase,
    MetaParameterized,
)
