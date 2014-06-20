# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozinfo
import os
import platform
import sys

from .runner import BaseRunner


class GeckoRuntimeRunner(BaseRunner):
    """
    The base runner class used for local gecko runtime binaries,
    such as Firefox and Thunderbird.
    """

    def __init__(self, binary, cmdargs=None, **runner_args):
        BaseRunner.__init__(self, **runner_args)

        self.binary = binary
        self.cmdargs = cmdargs or []

        # allows you to run an instance of Firefox separately from any other instances
        self.env['MOZ_NO_REMOTE'] = '1'
        # keeps Firefox attached to the terminal window after it starts
        self.env['NO_EM_RESTART'] = '1'

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

        # If running on OS X 10.5 or older, wrap |cmd| so that it will
        # be executed as an i386 binary, in case it's a 32-bit/64-bit universal
        # binary.
        if mozinfo.isMac and hasattr(platform, 'mac_ver') and \
                platform.mac_ver()[0][:4] < '10.6':
            command = ["arch", "-arch", "i386"] + command

        if hasattr(self.app_ctx, 'wrap_command'):
            command = self.app_ctx.wrap_command(command)
        return command

    def start(self, *args, **kwargs):
        # ensure the profile exists
        if not self.profile.exists():
            self.profile.reset()
            assert self.profile.exists(), "%s : failure to reset profile" % self.__class__.__name__

        BaseRunner.start(self, *args, **kwargs)
