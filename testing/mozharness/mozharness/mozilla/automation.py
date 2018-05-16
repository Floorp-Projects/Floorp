#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Code to integration with automation.
"""

import os

try:
    import simplejson as json
    assert json
except ImportError:
    import json

from mozharness.base.log import INFO, WARNING, ERROR

TBPL_SUCCESS = 'SUCCESS'
TBPL_WARNING = 'WARNING'
TBPL_FAILURE = 'FAILURE'
TBPL_EXCEPTION = 'EXCEPTION'
TBPL_RETRY = 'RETRY'
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
TBPL_WORST_LEVEL_TUPLE = (TBPL_RETRY, TBPL_EXCEPTION, TBPL_FAILURE,
                          TBPL_WARNING, TBPL_SUCCESS)


class AutomationMixin(object):
    worst_status = TBPL_SUCCESS
    properties = {}

    def tryserver_email(self):
        pass

    def record_status(self, tbpl_status, level=None, set_return_code=True):
        if tbpl_status not in TBPL_STATUS_DICT:
            self.error("record_status() doesn't grok the status %s!" %
                       tbpl_status)
        else:
            if not level:
                level = TBPL_STATUS_DICT[tbpl_status]
            self.worst_status = self.worst_level(tbpl_status,
                                                 self.worst_status,
                                                 TBPL_WORST_LEVEL_TUPLE)
            if self.worst_status != tbpl_status:
                self.info("Current worst status %s is worse; keeping it." %
                          self.worst_status)
            self.add_summary("# TBPL %s #" % self.worst_status, level=level)
            if set_return_code:
                self.return_code = EXIT_STATUS_DICT[self.worst_status]

    def set_property(self, prop_name, prop_value, write_to_file=False):
        self.info("Setting property %s to %s" % (prop_name, prop_value))
        self.properties[prop_name] = prop_value
        if write_to_file:
            return self.dump_properties(prop_list=[prop_name],
                                        file_name=prop_name)
        return self.properties[prop_name]

    def query_property(self, prop_name):
        return self.properties.get(prop_name)

    def query_is_nightly(self):
        """returns whether or not the script should run as a nightly build."""
        return bool(self.config.get('nightly_build'))

    def dump_properties(self, prop_list=None, file_name="properties",
                        error_level=ERROR):
        c = self.config
        if not os.path.isabs(file_name):
            file_name = os.path.join(c['base_work_dir'], "properties",
                                     file_name)
        dir_name = os.path.dirname(file_name)
        if not os.path.isdir(dir_name):
            self.mkdir_p(dir_name)
        if not prop_list:
            prop_list = self.properties.keys()
            self.info("Writing properties to %s" % file_name)
        else:
            if not isinstance(prop_list, (list, tuple)):
                self.log("dump_properties: Can't dump non-list prop_list %s!" %
                         str(prop_list), level=error_level)
                return
            self.info("Writing properties %s to %s" % (str(prop_list),
                                                       file_name))
        contents = ""
        for prop in prop_list:
            contents += "%s:%s\n" % (prop, self.properties.get(prop, "None"))
        return self.write_to_file(file_name, contents)
