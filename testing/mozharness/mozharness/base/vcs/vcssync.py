#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Generic VCS support.
"""

import os
import smtplib
import sys
import time

sys.path.insert(1, os.path.dirname(os.path.dirname(os.path.dirname(sys.path[0]))))

from mozharness.base.log import ERROR, INFO
from mozharness.base.vcs.vcsbase import VCSScript


# VCSSyncScript {{{1
class VCSSyncScript(VCSScript):
    start_time = time.time()

    def __init__(self, **kwargs):
        super(VCSSyncScript, self).__init__(**kwargs)

    def notify(self, message=None, fatal=False):
        """ Email people in the notify_config (depending on status and failure_only)
            """
        c = self.config
        dirs = self.query_abs_dirs()
        job_name = c.get('job_name', c.get('conversion_dir', os.getcwd()))
        end_time = time.time()
        seconds = int(end_time - self.start_time)
        self.info("Job took %d seconds." % seconds)
        subject = "[vcs2vcs] Successful conversion for %s" % job_name
        text = ''
        error_contents = ''
        max_log_sample_size = c.get('email_max_log_sample_size') # default defined in vcs_sync.py
        error_log = os.path.join(dirs['abs_log_dir'], self.log_obj.log_files[ERROR])
        info_log = os.path.join(dirs['abs_log_dir'], self.log_obj.log_files[INFO])
        if os.path.exists(error_log) and os.path.getsize(error_log) > 0:
            error_contents = self.get_output_from_command(
                ["egrep", "-C5", "^[0-9:]+ +(ERROR|CRITICAL|FATAL) -", info_log],
                silent=True,
            )
        if fatal:
            subject = "[vcs2vcs] Failed conversion for %s" % job_name
            text = ''
            if len(message) > max_log_sample_size:
                text += '*** Message below has been truncated: it was %s characters, and has been reduced to %s characters:\n\n' % (len(message), max_log_sample_size)
            text += message[0:max_log_sample_size] + '\n\n' # limit message to max_log_sample_size in size (large emails fail to send)
        if not self.successful_repos:
            subject = "[vcs2vcs] Successful no-op conversion for %s" % job_name
        if error_contents and not fatal:
            subject += " with warnings"
        if self.successful_repos:
            if len(self.successful_repos) <= 5:
                subject += ' (' + ','.join(self.successful_repos) + ')'
            else:
                text += "Successful repos: %s\n\n" % ', '.join(self.successful_repos)
        subject += ' (%ds)' % seconds
        if self.summary_list:
            text += 'Summary is non-zero:\n\n'
            for item in self.summary_list:
                text += '%s - %s\n' % (item['level'], item['message'])
        if not fatal and error_contents and not self.summary_list:
            text += 'Summary is empty; the below errors have probably been auto-corrected.\n\n'
        if error_contents:
            if len(error_contents) > max_log_sample_size:
                text += '\n*** Message below has been truncated: it was %s characters, and has been reduced to %s characters:\n' % (len(error_contents), max_log_sample_size)
            text += '\n%s\n\n' % error_contents[0:max_log_sample_size] # limit message to 100KB in size (large emails fail to send)
        if not text:
            subject += " <EOM>"
        for notify_config in c.get('notify_config', []):
            if not fatal:
                if notify_config.get('failure_only'):
                    self.info("Skipping notification for %s (failure_only)" % notify_config['to'])
                    continue
                if not text and notify_config.get('skip_empty_messages'):
                    self.info("Skipping notification for %s (skip_empty_messages)" % notify_config['to'])
                    continue
            fromaddr = notify_config.get('from', c['default_notify_from'])
            message = '\r\n'.join((
                "From: %s" % fromaddr,
                "To: %s" % notify_config['to'],
                "CC: %s" % ','.join(notify_config.get('cc', [])),
                "Subject: %s" % subject,
                "",
                text
            ))
            toaddrs = [notify_config['to']] + notify_config.get('cc', [])
            # TODO allow for a different smtp server
            # TODO deal with failures
            server = smtplib.SMTP('localhost')
            self.retry(
                server.sendmail,
                args=(fromaddr, toaddrs, message),
            )
            server.quit()
