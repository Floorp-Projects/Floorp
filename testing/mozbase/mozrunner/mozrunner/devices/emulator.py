# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from telnetlib import Telnet
import datetime
import os
import shutil
import subprocess
import tempfile
import time

from mozdevice import ADBHost
from mozprocess import ProcessHandler

from .base import Device
from .emulator_battery import EmulatorBattery
from .emulator_geo import EmulatorGeo
from .emulator_screen import EmulatorScreen
from ..errors import TimeoutException


class ArchContext(object):

    def __init__(self, arch, context, binary=None, avd=None, extra_args=None):
        homedir = getattr(context, 'homedir', '')
        kernel = os.path.join(homedir, 'prebuilts', 'qemu-kernel', '%s', '%s')
        sysdir = os.path.join(homedir, 'out', 'target', 'product', '%s')
        self.extra_args = []
        self.binary = os.path.join(context.bindir or '', 'emulator')
        if arch == 'x86':
            self.binary = os.path.join(context.bindir or '', 'emulator-x86')
            self.kernel = kernel % ('x86', 'kernel-qemu')
            self.sysdir = sysdir % 'generic_x86'
        elif avd:
            self.avd = avd
            self.extra_args = [
                '-show-kernel', '-debug',
                'init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket'
            ]
        else:
            self.kernel = kernel % ('arm', 'kernel-qemu-armv7')
            self.sysdir = sysdir % 'generic'
            self.extra_args = ['-cpu', 'cortex-a8']

        if binary:
            self.binary = binary

        if extra_args:
            self.extra_args.extend(extra_args)


class SDCard(object):

    def __init__(self, emulator, size):
        self.emulator = emulator
        self.path = self.create_sdcard(size)

    def create_sdcard(self, sdcard_size):
        """
        Creates an sdcard partition in the emulator.

        :param sdcard_size: Size of partition to create, e.g '10MB'.
        """
        mksdcard = self.emulator.app_ctx.which('mksdcard')
        path = tempfile.mktemp(prefix='sdcard', dir=self.emulator.tmpdir)
        sdargs = [mksdcard, '-l', 'mySdCard', sdcard_size, path]
        sd = subprocess.Popen(sdargs, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)
        retcode = sd.wait()
        if retcode:
            raise Exception('unable to create sdcard: exit code %d: %s'
                            % (retcode, sd.stdout.read()))
        return path


class BaseEmulator(Device):
    port = None
    proc = None
    telnet = None

    def __init__(self, app_ctx, **kwargs):
        self.arch = ArchContext(kwargs.pop('arch', 'arm'), app_ctx,
                                binary=kwargs.pop('binary', None),
                                avd=kwargs.pop('avd', None))
        super(BaseEmulator, self).__init__(app_ctx, **kwargs)
        self.tmpdir = tempfile.mkdtemp()
        # These rely on telnet
        self.battery = EmulatorBattery(self)
        self.geo = EmulatorGeo(self)
        self.screen = EmulatorScreen(self)

    @property
    def args(self):
        """
        Arguments to pass into the emulator binary.
        """
        return [self.arch.binary]

    def start(self):
        """
        Starts a new emulator.
        """
        if self.proc:
            return

        original_devices = set(self._get_online_devices())

        # QEMU relies on atexit() to remove temporary files, which does not
        # work since mozprocess uses SIGKILL to kill the emulator process.
        # Use a customized temporary directory so we can clean it up.
        os.environ['ANDROID_TMP'] = self.tmpdir

        qemu_log = None
        qemu_proc_args = {}
        if self.logdir:
            # save output from qemu to logfile
            qemu_log = os.path.join(self.logdir, 'qemu.log')
            if os.path.isfile(qemu_log):
                self._rotate_log(qemu_log)
            qemu_proc_args['logfile'] = qemu_log
        else:
            qemu_proc_args['processOutputLine'] = lambda line: None
        self.proc = ProcessHandler(self.args, **qemu_proc_args)
        self.proc.run()

        devices = set(self._get_online_devices())
        now = datetime.datetime.now()
        while (devices - original_devices) == set([]):
            time.sleep(1)
            # Sometimes it takes more than 60s to launch emulator, so we
            # increase timeout value to 180s. Please see bug 1143380.
            if datetime.datetime.now() - now > datetime.timedelta(
                    seconds=180):
                raise TimeoutException(
                    'timed out waiting for emulator to start')
            devices = set(self._get_online_devices())
        devices = devices - original_devices
        self.serial = devices.pop()
        self.connect()

    def _get_online_devices(self):
        adbhost = ADBHost()
        return [d['device_serial'] for d in adbhost.devices() if d['state'] != 'offline' if
                d['device_serial'].startswith('emulator')]

    def connect(self):
        """
        Connects to a running device. If no serial was specified in the
        constructor, defaults to the first entry in `adb devices`.
        """
        if self.connected:
            return

        super(BaseEmulator, self).connect()
        self.port = int(self.serial[self.serial.rindex('-') + 1:])

    def cleanup(self):
        """
        Cleans up and kills the emulator, if it was started by mozrunner.
        """
        super(BaseEmulator, self).cleanup()
        if self.proc:
            self.proc.kill()
            self.proc = None
            self.connected = False

        # Remove temporary files
        if os.path.isdir(self.tmpdir):
            shutil.rmtree(self.tmpdir)

    def _get_telnet_response(self, command=None):
        output = []
        assert self.telnet
        if command is not None:
            self.telnet.write('%s\n' % command)
        while True:
            line = self.telnet.read_until('\n')
            output.append(line.rstrip())
            if line.startswith('OK'):
                return output
            elif line.startswith('KO:'):
                raise Exception('bad telnet response: %s' % line)

    def _run_telnet(self, command):
        if not self.telnet:
            self.telnet = Telnet('localhost', self.port)
            self._get_telnet_response()
        return self._get_telnet_response(command)

    def __del__(self):
        if self.telnet:
            self.telnet.write('exit\n')
            self.telnet.read_all()


class EmulatorAVD(BaseEmulator):

    def __init__(self, app_ctx, binary, avd, port=5554, **kwargs):
        super(EmulatorAVD, self).__init__(app_ctx, binary=binary, avd=avd, **kwargs)
        self.port = port

    @property
    def args(self):
        """
        Arguments to pass into the emulator binary.
        """
        qemu_args = super(EmulatorAVD, self).args
        qemu_args.extend(['-avd', self.arch.avd,
                          '-port', str(self.port)])
        qemu_args.extend(self.arch.extra_args)
        return qemu_args

    def start(self):
        if self.proc:
            return

        env = os.environ
        env['ANDROID_AVD_HOME'] = self.app_ctx.avd_home

        super(EmulatorAVD, self).start()
