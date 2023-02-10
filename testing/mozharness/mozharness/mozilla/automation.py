#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Code to integration with automation.
"""

try:
    import simplejson as json

    assert json
except ImportError:
    import json

from mozharness.base.log import ERROR, INFO, WARNING

TBPL_SUCCESS = "SUCCESS"
TBPL_WARNING = "WARNING"
TBPL_FAILURE = "FAILURE"
TBPL_EXCEPTION = "EXCEPTION"
TBPL_RETRY = "RETRY"
TBPL_STATUS_DICT = {
    TBPL_SUCCESS: INFO,
    TBPL_WARNING: WARNING,
    TBPL_FAILURE: ERROR,
    TBPL_EXCEPTION: ERROR,
    TBPL_RETRY: WARNING,
}
EXIT_STATUS_DICT = {
    TBPL_SUCCESS: 0,
    TBPL_WARNING: 1,
    TBPL_FAILURE: 2,
    TBPL_EXCEPTION: 3,
    TBPL_RETRY: 4,
}
TBPL_WORST_LEVEL_TUPLE = (
    TBPL_RETRY,
    TBPL_EXCEPTION,
    TBPL_FAILURE,
    TBPL_WARNING,
    TBPL_SUCCESS,
)


class AutomationMixin(object):
    worst_status = TBPL_SUCCESS
    properties = {}

    def tryserver_email(self):
        pass

    def record_status(self, tbpl_status, level=None, set_return_code=True):
        if tbpl_status not in TBPL_STATUS_DICT:
            self.error("record_status() doesn't grok the status %s!" % tbpl_status)
        else:
            if not level:
                level = TBPL_STATUS_DICT[tbpl_status]
            self.worst_status = self.worst_level(
                tbpl_status, self.worst_status, TBPL_WORST_LEVEL_TUPLE
            )
            if self.worst_status != tbpl_status:
                self.info(
                    "Current worst status %s is worse; keeping it." % self.worst_status
                )
            if set_return_code:
                self.return_code = EXIT_STATUS_DICT[self.worst_status]

    def add_failure(self, key, message="%(key)s failed.", level=ERROR):
        if key not in self.failures:
            self.failures.append(key)
            self.add_summary(message % {"key": key}, level=level)
            self.record_status(TBPL_FAILURE)

    def query_failure(self, key):
        return key in self.failures

    def query_is_nightly(self):
        """returns whether or not the script should run as a nightly build."""
        return bool(self.config.get("nightly_build"))
