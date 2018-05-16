#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Code to tie into buildbot.
Ideally this will go away if and when we retire buildbot.
"""

import copy
import os
import re
import sys

try:
    import simplejson as json
    assert json
except ImportError:
    import json

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.config import parse_config_file
from mozharness.base.log import INFO, WARNING, ERROR

# BuildbotMixin {{{1

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


class BuildbotMixin(object):
    buildbot_config = None
    buildbot_properties = {}
    worst_buildbot_status = TBPL_SUCCESS

    def tryserver_email(self):
        pass

    def buildbot_status(self, tbpl_status, level=None, set_return_code=True):
        if tbpl_status not in TBPL_STATUS_DICT:
            self.error("buildbot_status() doesn't grok the status %s!" % tbpl_status)
        else:
            # Set failure if our log > buildbot_max_log_size (bug 876159)
            if self.config.get("buildbot_max_log_size") and self.log_obj:
                # Find the path to the default log
                dirs = self.query_abs_dirs()
                log_file = os.path.join(
                    dirs['abs_log_dir'],
                    self.log_obj.log_files[self.log_obj.log_level]
                )
                if os.path.exists(log_file):
                    file_size = os.path.getsize(log_file)
                    if file_size > self.config['buildbot_max_log_size']:
                        self.error("Log file size %d is greater than max allowed %d! Setting TBPL_FAILURE (was %s)..." % (file_size, self.config['buildbot_max_log_size'], tbpl_status))
                        tbpl_status = TBPL_FAILURE
            if not level:
                level = TBPL_STATUS_DICT[tbpl_status]
            self.worst_buildbot_status = self.worst_level(tbpl_status, self.worst_buildbot_status, TBPL_WORST_LEVEL_TUPLE)
            if self.worst_buildbot_status != tbpl_status:
                self.info("Current worst status %s is worse; keeping it." % self.worst_buildbot_status)
            self.add_summary("# TBPL %s #" % self.worst_buildbot_status, level=level)
            if set_return_code:
                self.return_code = EXIT_STATUS_DICT[self.worst_buildbot_status]

    def set_buildbot_property(self, prop_name, prop_value, write_to_file=False):
        self.info("Setting buildbot property %s to %s" % (prop_name, prop_value))
        self.buildbot_properties[prop_name] = prop_value
        if write_to_file:
            return self.dump_buildbot_properties(prop_list=[prop_name], file_name=prop_name)
        return self.buildbot_properties[prop_name]

    def query_buildbot_property(self, prop_name):
        return self.buildbot_properties.get(prop_name)

    def query_is_nightly(self):
        """returns whether or not the script should run as a nightly build.

           First will check for 'nightly_build' in self.config and if that is
           not True, we will also allow buildbot_config to determine
           for us. Failing all of that, we default to False.
           Note, dependancy on buildbot_config is being deprecated.
           Putting everything in self.config is the preference.
        """
        if self.config.get('nightly_build'):
            return True
        elif self.buildbot_config and 'properties' in self.buildbot_config:
            return self.buildbot_config['properties'].get('nightly_build', False)
        else:
            return False

    def dump_buildbot_properties(self, prop_list=None, file_name="properties", error_level=ERROR):
        c = self.config
        if not os.path.isabs(file_name):
            file_name = os.path.join(c['base_work_dir'], "properties", file_name)
        dir_name = os.path.dirname(file_name)
        if not os.path.isdir(dir_name):
            self.mkdir_p(dir_name)
        if not prop_list:
            prop_list = self.buildbot_properties.keys()
            self.info("Writing buildbot properties to %s" % file_name)
        else:
            if not isinstance(prop_list, (list, tuple)):
                self.log("dump_buildbot_properties: Can't dump non-list prop_list %s!" % str(prop_list), level=error_level)
                return
            self.info("Writing buildbot properties %s to %s" % (str(prop_list), file_name))
        contents = ""
        for prop in prop_list:
            contents += "%s:%s\n" % (prop, self.buildbot_properties.get(prop, "None"))
        return self.write_to_file(file_name, contents)

    def query_who(self):
        """ looks for who triggered the build with a change.

        This is used for things like try builds where the upload dir is
        associated with who pushed to try. First it will look in self.config
        and failing that, will poll buildbot_config
        If nothing is found, it will default to returning "nobody@example.com"
        """
        if self.config.get('who'):
            return self.config['who']
        self.read_buildbot_config()
        try:
            return self.buildbot_config['sourcestamp']['changes'][0]['who']
        except (KeyError, IndexError):
            # KeyError: "sourcestamp" or "changes" or "who" not in buildbot_config
            # IndexError: buildbot_config['sourcestamp']['changes'] is empty
            pass
        try:
            return str(self.buildbot_config['properties']['who'])
        except KeyError:
            pass
        return "nobody@example.com"
