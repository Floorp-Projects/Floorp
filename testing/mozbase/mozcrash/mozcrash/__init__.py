# flake8: noqa
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
"""
mozcrash is a library for getting a stack trace out of processes that have crashed
and left behind a minidump file using the Google Breakpad library.
"""
from __future__ import absolute_import

from .mozcrash import *
