# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import mozinfo
import os
import sys

from .runner import BaseRunner


class GeckoRuntimeRunner(BaseRunner):
    """
    The base runner class used for local gecko runtime binaries,
    such as Firefox and Thunderbird.
    """

    def __init__(self, binary, cmdargs=None, **runner_args):
        self.show_crash_reporter = runner_args.pop('show_crash_reporter', False)
        BaseRunner.__init__(self, **runner_args)

        self.binary = binary
        self.cmdargs = cmdargs or []

        # allows you to run an instance of Firefox separately from any other instances
        self.env['MOZ_NO_REMOTE'] = '1'
        # keeps Firefox attached to the terminal window after it starts
        self.env['NO_EM_RESTART'] = '1'

        # Disable crash reporting dialogs that interfere with debugging
        self.env['GNOME_DISABLE_CRASH_DIALOG'] = '1'
        self.env['XRE_NO_WINDOWS_CRASH_DIALOG'] = '1'

        # set the library path if needed on linux
        if sys.platform == 'linux2' and self.binary.endswith('-bin'):
            dirname = os.path.dirname(self.binary)
            if os.environ.get('LD_LIBRARY_PATH', None):
                self.env['LD_LIBRARY_PATH'] = '%s:%s' % (os.environ['LD_LIBRARY_PATH'], dirname)
            else:
                self.env['LD_LIBRARY_PATH'] = dirname

    @property
    def command(self):
        command = [self.binary, '-profile', self.profile.profile]

        _cmdargs = [i for i in self.cmdargs
                    if i != '-foreground']
        if len(_cmdargs) != len(self.cmdargs):
            # foreground should be last; see
            # https://bugzilla.mozilla.org/show_bug.cgi?id=625614
            self.cmdargs = _cmdargs
            self.cmdargs.append('-foreground')
        if mozinfo.isMac and '-foreground' not in self.cmdargs:
            # runner should specify '-foreground' on Mac; see
            # https://bugzilla.mozilla.org/show_bug.cgi?id=916512
            self.cmdargs.append('-foreground')

        # Bug 775416 - Ensure that binary options are passed in first
        command[1:1] = self.cmdargs
        return command

    def start(self, *args, **kwargs):
        # ensure the profile exists
        if not self.profile.exists():
            self.profile.reset()
            assert self.profile.exists(), "%s : failure to reset profile" % self.__class__.__name__

        has_debugger = "debug_args" in kwargs and kwargs["debug_args"]
        if has_debugger:
            self.env["MOZ_CRASHREPORTER_DISABLE"] = "1"
        else:
            if not self.show_crash_reporter:
                # hide the crash reporter window
                self.env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
            self.env["MOZ_CRASHREPORTER"] = "1"

        BaseRunner.start(self, *args, **kwargs)


class BlinkRuntimeRunner(BaseRunner):
    """A base runner class for running apps like Google Chrome or Chromium."""
    def __init__(self, binary, cmdargs=None, **runner_args):
        super(BlinkRuntimeRunner, self).__init__(**runner_args)
        self.binary = binary
        self.cmdargs = cmdargs or []

        data_dir, name = os.path.split(self.profile.profile)
        profile_args = [
            '--user-data-dir={}'.format(data_dir),
            '--profile-directory={}'.format(name),
            '--no-first-run',
        ]
        self.cmdargs.extend(profile_args)

    @property
    def command(self):
        cmd = self.cmdargs[:]
        if self.profile.addons:
            cmd.append('--load-extension={}'.format(','.join(self.profile.addons)))
        return [self.binary] + cmd

    def check_for_crashes(self, *args, **kwargs):
        raise NotImplementedError
