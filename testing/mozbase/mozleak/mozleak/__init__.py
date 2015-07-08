# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
mozleak is a library for extracting memory leaks from leak logs files.
"""

from .leaklog import process_leak_log
