#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile

from mozharness.base.script import (
    PreScriptAction,
    PostScriptAction,
)

code_coverage_config_options = [
    [["--code-coverage"],
     {"action": "store_true",
      "dest": "code_coverage",
      "default": False,
      "help": "Whether test run should package and upload code coverage data."
      }],
]


class CodeCoverageMixin(object):
    """
    Mixin for setting GCOV_PREFIX during test execution, packaging up
    the resulting .gcda files and uploading them to blobber.
    """
    gcov_dir = None

    @property
    def code_coverage_enabled(self):
        try:
            if self.config.get('code_coverage'):
                return True

            # XXX workaround because bug 1110465 is hard
            return self.buildbot_config['properties']['stage_platform'] in ('linux64-ccov',)
        except (AttributeError, KeyError, TypeError):
            return False


    @PreScriptAction('run-tests')
    def _set_gcov_prefix(self, action):
        if not self.code_coverage_enabled:
            return
        self.gcov_dir = tempfile.mkdtemp()
        os.environ['GCOV_PREFIX'] = self.gcov_dir

    @PostScriptAction('run-tests')
    def _package_coverage_data(self, action, success=None):
        if not self.code_coverage_enabled:
            return
        del os.environ['GCOV_PREFIX']

        # TODO This is fragile, find rel_topsrcdir properly somehow
        # We need to find the path relative to the gecko topsrcdir. Use
        # some known gecko directories as a test.
        canary_dirs = ['browser', 'docshell', 'dom', 'js', 'layout', 'toolkit', 'xpcom', 'xpfe']
        rel_topsrcdir = None
        for root, dirs, files in os.walk(self.gcov_dir):
            # need to use 'any' in case no gcda data was generated in that subdir.
            if any(d in dirs for d in canary_dirs):
                rel_topsrcdir = root
                break
        else:
            # Unable to upload code coverage files. Since this is the whole
            # point of code coverage, making this fatal.
            self.fatal("Could not find relative topsrcdir in code coverage "
                       "data!")

        dirs = self.query_abs_dirs()
        file_path = os.path.join(
            dirs['abs_blob_upload_dir'], 'code-coverage-gcda.zip')
        command = ['zip', '-r', file_path, '.']
        self.run_command(command, cwd=rel_topsrcdir)
        shutil.rmtree(self.gcov_dir)
