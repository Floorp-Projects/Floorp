#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Generic ways to upload + download files.
"""

import pprint

try:
    from urllib2 import urlopen
except ImportError:
    from urllib.request import urlopen

import json

from mozharness.base.log import DEBUG


# TransferMixin {{{1
class TransferMixin(object):
    """
    Generic transfer methods.

    Dependent on BaseScript.
    """

    def load_json_from_url(self, url, timeout=30, log_level=DEBUG):
        self.log(
            "Attempting to download %s; timeout=%i" % (url, timeout), level=log_level
        )
        try:
            r = urlopen(url, timeout=timeout)
            j = json.load(r)
            self.log(pprint.pformat(j), level=log_level)
        except BaseException:
            self.exception(message="Unable to download %s!" % url)
            raise
        return j
