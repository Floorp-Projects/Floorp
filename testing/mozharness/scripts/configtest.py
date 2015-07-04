#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""configtest.py

Verify the .json and .py files in the configs/ directory are well-formed.
Further tests to verify validity would be desirable.

This is also a good example script to look at to understand mozharness.
"""

import os
import pprint
import sys
try:
    import simplejson as json
except ImportError:
    import json

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import BaseScript

# ConfigTest {{{1
class ConfigTest(BaseScript):
    config_options = [[
     ["--test-file",],
     {"action": "extend",
      "dest": "test_files",
      "help": "Specify which config files to test"
     }
    ]]

    def __init__(self, require_config_file=False):
        self.config_files = []
        BaseScript.__init__(self, config_options=self.config_options,
                            all_actions=['list-config-files',
                                         'test-json-configs',
                                         'test-python-configs',
                                         'summary',
                                         ],
                            default_actions=['test-json-configs',
                                             'test-python-configs',
                                             'summary',
                                            ],
                            require_config_file=require_config_file)

    def query_config_files(self):
        """This query method, much like others, caches its runtime
        settings in self.VAR so we don't have to figure out config_files
        multiple times.
        """
        if self.config_files:
            return self.config_files
        c = self.config
        if 'test_files' in c:
            self.config_files = c['test_files']
            return self.config_files
        self.debug("No --test-file(s) specified; defaulting to crawling the configs/ directory.")
        config_files = []
        for root, dirs, files in os.walk(os.path.join(sys.path[0], "..",
                                                      "configs")):
            for name in files:
                # Hardcode =P
                if name.endswith(".json") or name.endswith(".py"):
                    if not name.startswith("test_malformed"):
                        config_files.append(os.path.join(root, name))
        self.config_files = config_files
        return self.config_files

    def list_config_files(self):
        """ Non-default action that is mainly here to demonstrate how
        non-default actions work in a mozharness script.
        """
        config_files = self.query_config_files()
        for config_file in config_files:
            self.info(config_file)

    def test_json_configs(self):
        """ Currently only "is this well-formed json?"

        """
        config_files = self.query_config_files()
        filecount = [0, 0]
        for config_file in config_files:
            if config_file.endswith(".json"):
                filecount[0] += 1
                self.info("Testing %s." % config_file)
                contents = self.read_from_file(config_file, verbose=False)
                try:
                    json.loads(contents)
                except ValueError:
                    self.add_summary("%s is invalid json." % config_file,
                                     level="error")
                    self.error(pprint.pformat(sys.exc_info()[1]))
                else:
                    self.info("Good.")
                    filecount[1] += 1
        if filecount[0]:
            self.add_summary("%d of %d json config files were good." %
                             (filecount[1], filecount[0]))
        else:
            self.add_summary("No json config files to test.")

    def test_python_configs(self):
        """Currently only "will this give me a config dictionary?"

        """
        config_files = self.query_config_files()
        filecount = [0, 0]
        for config_file in config_files:
            if config_file.endswith(".py"):
                filecount[0] += 1
                self.info("Testing %s." % config_file)
                global_dict = {}
                local_dict = {}
                try:
                    execfile(config_file, global_dict, local_dict)
                except:
                    self.add_summary("%s is invalid python." % config_file,
                                     level="error")
                    self.error(pprint.pformat(sys.exc_info()[1]))
                else:
                    if 'config' in local_dict and isinstance(local_dict['config'], dict):
                        self.info("Good.")
                        filecount[1] += 1
                    else:
                        self.add_summary("%s is valid python, but doesn't create a config dictionary." %
                                         config_file, level="error")
        if filecount[0]:
            self.add_summary("%d of %d python config files were good." %
                             (filecount[1], filecount[0]))
        else:
            self.add_summary("No python config files to test.")

# __main__ {{{1
if __name__ == '__main__':
    config_test = ConfigTest()
    config_test.run_and_exit()
