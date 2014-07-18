# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import datetime
import re
import signal
import sys
import tempfile
import time

from .runner import BaseRunner
from ..devices import Emulator

class DeviceRunner(BaseRunner):
    """
    The base runner class used for running gecko on
    remote devices (or emulators), such as B2G.
    """
    def __init__(self, device_class, device_args=None, **kwargs):
        process_args = {'stream': sys.stdout,
                        'processOutputLine': self.on_output,
                        'onTimeout': self.on_timeout }
        process_args.update(kwargs.get('process_args') or {})

        kwargs['process_args'] = process_args
        BaseRunner.__init__(self, **kwargs)

        device_args = device_args or {}
        self.device = device_class(**device_args)

        process_log = tempfile.NamedTemporaryFile(suffix='pidlog')
        self._env =  { 'MOZ_CRASHREPORTER': '1',
                       'MOZ_CRASHREPORTER_NO_REPORT': '1',
                       'MOZ_CRASHREPORTER_SHUTDOWN': '1',
                       'MOZ_HIDE_RESULTS_TABLE': '1',
                       'MOZ_PROCESS_LOG': process_log.name,
                       'NSPR_LOG_MODULES': 'signaling:5,mtransport:5,datachannel:5',
                       'R_LOG_LEVEL': '6',
                       'R_LOG_DESTINATION': 'stderr',
                       'R_LOG_VERBOSE': '1',
                       'NO_EM_RESTART': '1', }
        if kwargs.get('env'):
            self._env.update(kwargs['env'])

        # In this case we need to pass in env as part of the command.
        # Make this empty so runner doesn't pass anything into the
        # process class.
        self.env = None

    @property
    def command(self):
        cmd = [self.app_ctx.adb]
        if self.app_ctx.dm._deviceSerial:
            cmd.extend(['-s', self.app_ctx.dm._deviceSerial])
        cmd.append('shell')
        for k, v in self._env.iteritems():
            cmd.append('%s=%s' % (k, v))
        cmd.append(self.app_ctx.remote_binary)
        return cmd

    def start(self, *args, **kwargs):
        if isinstance(self.device, Emulator) and not self.device.connected:
            self.device.start()
        self.device.connect()
        self.device.setup_profile(self.profile)

        # TODO: this doesn't work well when the device is running but dropped
        # wifi for some reason. It would be good to probe the state of the device
        # to see if we have the homescreen running, or something, before waiting here
        self.device.wait_for_net()

        if not isinstance(self.device, Emulator):
            self.device.reboot()

        if not self.device.wait_for_net():
            raise Exception("Network did not come up when starting device")
        self.app_ctx.stop_application()

        BaseRunner.start(self, *args, **kwargs)

        timeout = 10 # seconds
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            if self.app_ctx.dm.processExist(self.app_ctx.remote_process):
                break
            time.sleep(1)
        else:
            print("timed out waiting for '%s' process to start" % self.app_ctx.remote_process)

        if not self.device.wait_for_net():
            raise Exception("Failed to get a network connection")

    def on_output(self, line):
        match = re.findall(r"TEST-START \| ([^\s]*)", line)
        if match:
            self.last_test = match[-1]

    def on_timeout(self):
        self.app_ctx.dm.killProcess(self.app_ctx.remote_process, sig=signal.SIGABRT)
        timeout = 10 # seconds
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            if not self.app_ctx.dm.processExist(self.app_ctx.remote_process):
                break
            time.sleep(1)
        else:
            print("timed out waiting for '%s' process to exit" % self.app_ctx.remote_process)

        msg = "DeviceRunner TEST-UNEXPECTED-FAIL | %s | application timed out after %s seconds"
        if self.timeout:
            timeout = self.timeout
        else:
            timeout = self.output_timeout
            msg = "%s with no output" % msg

        print(msg % (self.last_test, timeout))
        self.check_for_crashes()

    def check_for_crashes(self):
        dump_dir = self.device.pull_minidumps()
        BaseRunner.check_for_crashes(self, dump_directory=dump_dir, test_name=self.last_test)

    def cleanup(self, *args, **kwargs):
        BaseRunner.cleanup(self, *args, **kwargs)
        self.device.cleanup()
