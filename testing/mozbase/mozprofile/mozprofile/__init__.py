# flake8: noqa
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
To use mozprofile as an API you can import mozprofile.profile_ and/or the AddonManager_.

``mozprofile.profile`` features a generic ``Profile`` class.  In addition,
subclasses ``FirefoxProfile`` and ``ThundebirdProfile`` are available
with preset preferences for those applications.
"""

from __future__ import absolute_import

from mozprofile.addons import *
from mozprofile.cli import *
from mozprofile.diff import *
from mozprofile.permissions import *
from mozprofile.prefs import *
from mozprofile.profile import *
from mozprofile.view import *
