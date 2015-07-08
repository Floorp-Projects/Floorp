#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" mulet_unittest.py
This allows us to run Mulet unittests

Author: Armen Zambrano Gasparnian
"""
import os
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.transfer import TransferMixin
from mozharness.mozilla.gaia import GaiaMixin, gaia_config_options
from b2g_desktop_unittest import B2GDesktopTest

class MuletUnittest(B2GDesktopTest, GaiaMixin, TransferMixin):
    test_suites = ('reftest',)

    def __init__(self):
        super(MuletUnittest, self).__init__(options=gaia_config_options)

    def pull(self, **kwargs):
        GaiaMixin.pull(self, **kwargs)
        super(MuletUnittest, self).pull(**kwargs)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = super(MuletUnittest, self).query_abs_dirs()
        dirs = {}
        dirs['abs_gaia_dir'] = os.path.join(
                self.config["base_work_dir"], 'gaia')
        dirs['abs_gaia_profile'] = os.path.join(
                dirs["abs_gaia_dir"], 'profile')
        self.gaia_profile = dirs['abs_gaia_profile']
        abs_dirs.update(dirs)

        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def make_gaia(self, gaia_dir):
        self.run_command(self.query_exe('make', return_type="list"),
                         cwd=gaia_dir,
                         halt_on_failure=True)

    def preflight_run_tests(self):
        # This is if we don't run all actions since we set this inside
        # of download-and-extract()
        if self.config.get("binary_path"):
            self.binary_path = self.config["binary_path"]

        if not self.binary_path:
            self.fatal("Use --binary-path as it is needed for _query_abs_dir().")

        # This is if we don't run all actions since we set this inside
        # of download-and-extract()
        if not self.tree_config:
            self._read_tree_config()

    def run_tests(self):
        """
        Run the unit test suite.
        """
        dirs = self.query_abs_dirs()

        # NOTE: We currently don't clobber the checkout or the profile
        if not os.path.isdir(dirs['abs_gaia_profile']):
            # Make the gaia profile
            self.make_gaia(dirs['abs_gaia_dir'])

        super(MuletUnittest, self).run_tests()


# main {{{1
if __name__ == '__main__':
    mulet_unittest = MuletUnittest()
    mulet_unittest.run_and_exit()
