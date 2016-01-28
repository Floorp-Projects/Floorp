#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** BEGIN LICENSE BLOCK *****
"""firefox_media_tests_buildbot.py

Author: Maja Frydrychowicz
"""
import copy
import glob
import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.log import DEBUG, ERROR, INFO
from mozharness.base.script import PostScriptAction
from mozharness.mozilla.blob_upload import (
    BlobUploadMixin,
    blobupload_config_options
)
from mozharness.mozilla.buildbot import (
    TBPL_SUCCESS, TBPL_WARNING, TBPL_FAILURE
)
from mozharness.mozilla.testing.firefox_media_tests import (
    FirefoxMediaTestsBase, TESTFAILED, SUCCESS
)


class FirefoxMediaTestsBuildbot(FirefoxMediaTestsBase, BlobUploadMixin):

    def __init__(self):
        config_options = copy.deepcopy(blobupload_config_options)
        super(FirefoxMediaTestsBuildbot, self).__init__(
            config_options=config_options,
            all_actions=['clobber',
                         'read-buildbot-config',
                         'download-and-extract',
                         'create-virtualenv',
                         'install',
                         'run-media-tests',
                         ],
        )

    def run_media_tests(self):
        status = super(FirefoxMediaTestsBuildbot, self).run_media_tests()
        if status == SUCCESS:
            tbpl_status = TBPL_SUCCESS
        else:
            tbpl_status = TBPL_FAILURE
        if status == TESTFAILED:
            tbpl_status = TBPL_WARNING
        self.buildbot_status(tbpl_status)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(FirefoxMediaTestsBuildbot, self).query_abs_dirs()
        dirs = {
            'abs_blob_upload_dir': os.path.join(abs_dirs['abs_work_dir'],
                                                   'blobber_upload_dir')
        }
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def _query_cmd(self):
        cmd = super(FirefoxMediaTestsBuildbot, self)._query_cmd()
        dirs = self.query_abs_dirs()
        # configure logging
        blob_upload_dir = dirs.get('abs_blob_upload_dir')
        cmd += ['--gecko-log', os.path.join(blob_upload_dir, 'gecko.log')]
        cmd += ['--log-html', os.path.join(blob_upload_dir, 'media_tests.html')]
        cmd += ['--log-mach', os.path.join(blob_upload_dir, 'media_tests_mach.log')]
        return cmd

    @PostScriptAction('run-media-tests')
    def _collect_uploads(self, action, success=None):
        """ Copy extra (log) files to blob upload dir. """
        dirs = self.query_abs_dirs()
        log_dir = dirs.get('abs_log_dir')
        blob_upload_dir = dirs.get('abs_blob_upload_dir')
        if not log_dir or not blob_upload_dir:
            return
        self.mkdir_p(blob_upload_dir)
        # Move firefox-media-test screenshots into log_dir
        screenshots_dir = os.path.join(dirs['base_work_dir'],
                                       'screenshots')
        log_screenshots_dir = os.path.join(log_dir, 'screenshots')
        if os.access(log_screenshots_dir, os.F_OK):
            self.rmtree(log_screenshots_dir)
        if os.access(screenshots_dir, os.F_OK):
            self.move(screenshots_dir, log_screenshots_dir)

        # logs to upload: broadest level (info), error, screenshots
        uploads = glob.glob(os.path.join(log_screenshots_dir, '*'))
        log_files = self.log_obj.log_files
        log_level = self.log_obj.log_level

        def append_path(filename, dir=log_dir):
            if filename:
                uploads.append(os.path.join(dir, filename))

        append_path(log_files.get(ERROR))
        # never upload debug logs
        if log_level == DEBUG:
            append_path(log_files.get(INFO))
        else:
            append_path(log_files.get(log_level))
        # in case of SimpleFileLogger
        append_path(log_files.get('default'))
        for f in uploads:
            if os.access(f, os.F_OK):
                dest = os.path.join(blob_upload_dir, os.path.basename(f))
                self.copyfile(f, dest)


if __name__ == '__main__':
    media_test = FirefoxMediaTestsBuildbot()
    media_test.run_and_exit()
