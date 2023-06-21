# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
mozserve is a simple script that is used to launch test servers, and
is designed for use in mochitest and xpcshelltest.
"""

from .servers import DoHServer, Http2Server, Http3Server

__all__ = ["Http3Server", "Http2Server", "DoHServer"]
