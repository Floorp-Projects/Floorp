# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import datetime
import re
import signal
import sys
import tempfile
import time

import mozfile

from .runner import BaseRunner
from ..devices import BaseEmulator


class DeviceRunner(BaseRunner):
    """
    The base runner class used for running gecko on
    remote devices (or emulators).
    """
    env = {'MOZ_CRASHREPORTER': '1',
           'MOZ_CRASHREPORTER_NO_REPORT': '1',
           'MOZ_CRASHREPORTER_SHUTDOWN': '1',
           'MOZ_HIDE_RESULTS_TABLE': '1',
           'MOZ_LOG': 'signaling:3,mtransport:4,DataChannel:4,jsep:4',
           'R_LOG_LEVEL': '6',
           'R_LOG_DESTINATION': 'stderr',
           'R_LOG_VERBOSE': '1', }

    def __init__(self, device_class, device_args=None, **kwargs):
        process_log = tempfile.NamedTemporaryFile(suffix='pidlog')
        # the env will be passed to the device, it is not a *real* env
        self._device_env = dict(DeviceRunner.env)
        self._device_env['MOZ_PROCESS_LOG'] = process_log.name
        # be sure we do not pass env to the parent class ctor
        env = kwargs.pop('env', None)
        if env:
            self._device_env.update(env)

        process_args = {'stream': sys.stdout,
                        'processOutputLine': self.on_output,
                        'onFinish': self.on_finish,
                        'onTimeout': self.on_timeout}
        process_args.update(kwargs.get('process_args') or {})

        kwargs['process_args'] = process_args
        BaseRunner.__init__(self, **kwargs)

        device_args = device_args or {}
        self.device = device_class(**device_args)

    @property
    def command(self):
        # command built by mozdevice -- see start() below
        return None

    def start(self, *args, **kwargs):
        if isinstance(self.device, BaseEmulator) and not self.device.connected:
            self.device.start()
        self.device.connect()
        self.device.setup_profile(self.profile)

        app = self.app_ctx.remote_process
        args = ["-no-remote", "-profile", self.app_ctx.remote_profile]
        args.extend(self.cmdargs)
        env = self._device_env
        url = None
        if 'geckoview' in app:
            activity = "TestRunnerActivity"
            self.app_ctx.device.launch_activity(app, activity, e10s=True, moz_env=env,
                                                extra_args=args, url=url)
        else:
            self.app_ctx.device.launch_fennec(
                app, moz_env=env, extra_args=args, url=url)

        timeout = 10  # seconds
        end_time = datetime.datetime.now() + datetime.timedelta(seconds=timeout)
        while not self.is_running() and datetime.datetime.now() < end_time:
            time.sleep(.5)
        if not self.is_running():
            print("timed out waiting for '%s' process to start" % self.app_ctx.remote_process)

    def stop(self, sig=None):
        if not sig and self.is_running():
            self.app_ctx.stop_application()

        if self.is_running():
            timeout = 10

            self.app_ctx.device.pkill(self.app_ctx.remote_process, sig=sig)
            if self.wait(timeout) is None and sig is not None:
                print("timed out waiting for '%s' process to exit, trying "
                      "without signal {}".format(
                          self.app_ctx.remote_process, sig))

            # need to call adb stop otherwise the system will attempt to
            # restart the process
            self.app_ctx.stop_application()
            if self.wait(timeout) is None:
                print("timed out waiting for '%s' process to exit".format(
                    self.app_ctx.remote_process))

    @property
    def returncode(self):
        """The returncode of the remote process.

        A value of None indicates the process is still running. Otherwise 0 is
        returned, because there is no known way yet to retrieve the real exit code.
        """
        if self.app_ctx.device.process_exist(self.app_ctx.remote_process):
            return None

        return 0

    def wait(self, timeout=None):
        """Wait for the remote process to exit.

        :param timeout: if not None, will return after timeout seconds.

        :returns: the process return code or None if timeout was reached
                  and the process is still running.
        """
        end_time = None
        if timeout is not None:
            end_time = datetime.datetime.now() + datetime.timedelta(seconds=timeout)

        while self.is_running():
            if end_time is not None and datetime.datetime.now() > end_time:
                break
            time.sleep(.5)

        return self.returncode

    def on_output(self, line):
        match = re.findall(r"TEST-START \| ([^\s]*)", line)
        if match:
            self.last_test = match[-1]

    def on_timeout(self):
        self.stop(sig=signal.SIGABRT)
        msg = "DeviceRunner TEST-UNEXPECTED-FAIL | %s | application timed out after %s seconds"
        if self.timeout:
            timeout = self.timeout
        else:
            timeout = self.output_timeout
            msg = "%s with no output" % msg

        print(msg % (self.last_test, timeout))
        self.check_for_crashes()

    def on_finish(self):
        self.check_for_crashes()

    def check_for_crashes(self, dump_save_path=None, test_name=None, **kwargs):
        test_name = test_name or self.last_test
        dump_dir = self.device.pull_minidumps()
        crashed = BaseRunner.check_for_crashes(
            self,
            dump_directory=dump_dir,
            dump_save_path=dump_save_path,
            test_name=test_name,
            **kwargs)
        mozfile.remove(dump_dir)
        return crashed

    def cleanup(self, *args, **kwargs):
        BaseRunner.cleanup(self, *args, **kwargs)
        self.device.cleanup()


class FennecRunner(DeviceRunner):

    def __init__(self, cmdargs=None, **kwargs):
        super(FennecRunner, self).__init__(**kwargs)
        self.cmdargs = cmdargs or []
