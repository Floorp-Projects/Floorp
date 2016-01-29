#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** BEGIN LICENSE BLOCK *****
"""firefox_media_tests_jenkins.py

Author: Syd Polk
"""
import copy
import glob
import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import PreScriptAction
from mozharness.mozilla.testing.firefox_media_tests import (
    FirefoxMediaTestsBase
)


class FirefoxMediaTestsJenkins(FirefoxMediaTestsBase):

    def __init__(self):
        super(FirefoxMediaTestsJenkins, self).__init__(
            all_actions=['clobber',
                         'checkout',
                         'download-and-extract',
                         'create-virtualenv',
                         'install',
                         'run-media-tests',
                         ],
        )

    def _query_cmd(self):
        cmd = super(FirefoxMediaTestsJenkins, self)._query_cmd()

        dirs = self.query_abs_dirs()

        # configure logging
        log_dir = dirs.get('abs_log_dir')
        cmd += ['--gecko-log', os.path.join(log_dir, 'gecko.log')]
        cmd += ['--log-html', os.path.join(log_dir, 'media_tests.html')]
        cmd += ['--log-mach', os.path.join(log_dir, 'media_tests_mach.log')]

        return cmd

if __name__ == '__main__':
    media_test = FirefoxMediaTestsJenkins()
    media_test.run_and_exit()
