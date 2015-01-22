# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telnetlib import Telnet
import datetime
import os
import shutil
import subprocess
import sys
import tempfile
import time

from mozprocess import ProcessHandler

from .base import Device
from .emulator_battery import EmulatorBattery
from .emulator_geo import EmulatorGeo
from .emulator_screen import EmulatorScreen
from ..utils import uses_marionette
from ..errors import TimeoutException

class ArchContext(object):
    def __init__(self, arch, context, binary=None):
        kernel = os.path.join(context.homedir, 'prebuilts', 'qemu-kernel', '%s', '%s')
        sysdir = os.path.join(context.homedir, 'out', 'target', 'product', '%s')
        if arch == 'x86':
            self.binary = os.path.join(context.bindir, 'emulator-x86')
            self.kernel = kernel % ('x86', 'kernel-qemu')
            self.sysdir = sysdir % 'generic_x86'
            self.extra_args = []
        else:
            self.binary = os.path.join(context.bindir, 'emulator')
            self.kernel = kernel % ('arm', 'kernel-qemu-armv7')
            self.sysdir = sysdir % 'generic'
            self.extra_args = ['-cpu', 'cortex-a8']

        if binary:
            self.binary = binary


class Emulator(Device):
    logcat_proc = None
    port = None
    proc = None
    telnet = None

    def __init__(self, app_ctx, arch, resolution=None, sdcard=None, userdata=None,
                 no_window=None, binary=None, **kwargs):
        Device.__init__(self, app_ctx, **kwargs)

        self.arch = ArchContext(arch, self.app_ctx, binary=binary)
        self.resolution = resolution or '320x480'
        self.tmpdir = tempfile.mkdtemp()
        self.sdcard = None
        if sdcard:
            self.sdcard = self.create_sdcard(sdcard)
        self.userdata = tempfile.NamedTemporaryFile(prefix='userdata-qemu', dir=self.tmpdir)
        self.initdata = userdata if userdata else os.path.join(self.arch.sysdir, 'userdata.img')
        self.no_window = no_window

        self.battery = EmulatorBattery(self)
        self.geo = EmulatorGeo(self)
        self.screen = EmulatorScreen(self)

    @property
    def args(self):
        """
        Arguments to pass into the emulator binary.
        """
        qemu_args = [self.arch.binary,
                     '-kernel', self.arch.kernel,
                     '-sysdir', self.arch.sysdir,
                     '-data', self.userdata.name,
                     '-initdata', self.initdata,
                     '-wipe-data']
        if self.no_window:
            qemu_args.append('-no-window')
        if self.sdcard:
            qemu_args.extend(['-sdcard', self.sdcard])
        qemu_args.extend(['-memory', '512',
                          '-partition-size', '512',
                          '-verbose',
                          '-skin', self.resolution,
                          '-gpu', 'on',
                          '-qemu'] + self.arch.extra_args)
        return qemu_args

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
            if datetime.datetime.now() - now > datetime.timedelta(seconds=60):
                raise TimeoutException('timed out waiting for emulator to start')
            devices = set(self._get_online_devices())
        devices = devices - original_devices
        self.serial = devices.pop()
        self.connect()

    def _get_online_devices(self):
        return set([d[0] for d in self.dm.devices() if d[1] != 'offline' if d[0].startswith('emulator')])

    def connect(self):
        """
        Connects to a running device. If no serial was specified in the
        constructor, defaults to the first entry in `adb devices`.
        """
        if self.connected:
            return

        Device.connect(self)

        self.port = int(self.serial[self.serial.rindex('-')+1:])
        self.geo.set_default_location()
        self.screen.initialize()

        # setup DNS fix for networking
        self.app_ctx.dm.shellCheckOutput(['setprop', 'net.dns1', '10.0.2.3'])

    def create_sdcard(self, sdcard_size):
        """
        Creates an sdcard partition in the emulator.

        :param sdcard_size: Size of partition to create, e.g '10MB'.
        """
        mksdcard = self.app_ctx.which('mksdcard')
        path = tempfile.mktemp(prefix='sdcard', dir=self.tmpdir)
        sdargs = [mksdcard, '-l', 'mySdCard', sdcard_size, path]
        sd = subprocess.Popen(sdargs, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        retcode = sd.wait()
        if retcode:
            raise Exception('unable to create sdcard: exit code %d: %s'
                            % (retcode, sd.stdout.read()))
        return path

    def cleanup(self):
        """
        Cleans up and kills the emulator.
        """
        Device.cleanup(self)
        if self.proc:
            self.proc.kill()
            self.proc = None

        # Remove temporary files
        self.userdata.close()
        shutil.rmtree(self.tmpdir)

    # TODO this function is B2G specific and shouldn't live here
    @uses_marionette
    def wait_for_system_message(self, marionette):
        marionette.set_script_timeout(45000)
        # Telephony API's won't be available immediately upon emulator
        # boot; we have to wait for the syste-message-listener-ready
        # message before we'll be able to use them successfully.  See
        # bug 792647.
        print 'waiting for system-message-listener-ready...'
        try:
            marionette.execute_async_script("""
waitFor(
    function() { marionetteScriptFinished(true); },
    function() { return isSystemMessageListenerReady(); }
);
            """)
        except:
            # Look for ScriptTimeoutException this way to avoid a
            # dependency on the marionette python client.
            exc_name = sys.exc_info()[0].__name__
            if exc_name != 'ScriptTimeoutException':
                raise

            print 'timed out'
            # We silently ignore the timeout if it occurs, since
            # isSystemMessageListenerReady() isn't available on
            # older emulators.  45s *should* be enough of a delay
            # to allow telephony API's to work.
            pass
        print '...done'

    # TODO this function is B2G specific and shouldn't live here
    @uses_marionette
    def wait_for_homescreen(self, marionette):
        print 'waiting for homescreen...'

        marionette.set_context(marionette.CONTEXT_CONTENT)
        marionette.execute_async_script("""
log('waiting for mozbrowserloadend');
window.addEventListener('mozbrowserloadend', function loaded(aEvent) {
  log('received mozbrowserloadend for ' + aEvent.target.src);
  if (aEvent.target.src.indexOf('ftu') != -1 || aEvent.target.src.indexOf('homescreen') != -1 || aEvent.target.src.indexOf('verticalhome') != -1) {
    window.removeEventListener('mozbrowserloadend', loaded);
    marionetteScriptFinished();
  }
});""", script_timeout=300000)
        print '...done'

    def _get_telnet_response(self, command=None):
        output = []
        assert(self.telnet)
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
