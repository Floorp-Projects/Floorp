# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import psutil
import re
import shutil
import signal
import telnetlib
import time
import urllib2
from distutils.spawn import find_executable

from mozdevice import DeviceManagerADB, DMError
from mozprocess import ProcessHandler

EMULATOR_HOME_DIR = os.path.join(os.path.expanduser('~'), '.mozbuild', 'android-device')

TOOLTOOL_URL = 'https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py'

class AvdInfo(object):
    """
       Simple class to contain an AVD description.
    """
    def __init__(self, description, name, tooltool_manifest, extra_args,
                 port, uses_sut, sut_port, sut_port2):
        self.description = description
        self.name = name
        self.tooltool_manifest = tooltool_manifest
        self.extra_args = extra_args
        self.port = port
        self.uses_sut = uses_sut
        self.sut_port = sut_port
        self.sut_port2 = sut_port2


"""
   A dictionary to map an AVD type to a description of that type of AVD.

   There is one entry for each type of AVD used in Mozilla automated tests
   and the parameters for each reflect those used in mozharness.
"""
AVD_DICT = {
    '2.3': AvdInfo('Android 2.3',
                   'mozemulator-2.3',
                   'testing/config/tooltool-manifests/androidarm/releng.manifest',
                   ['-debug',
                    'init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket',
                    '-qemu', '-m', '1024', '-cpu', 'cortex-a9'],
                   5554,
                   True,
                   20701, 20700),
    '4.3': AvdInfo('Android 4.3',
                   'mozemulator-4.3',
                   'testing/config/tooltool-manifests/androidarm_4_3/releng.manifest',
                   ['-show-kernel', '-debug',
                    'init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket'],
                   5554,
                   False,
                   0, 0),
    'x86': AvdInfo('Android 4.2 x86',
                   'mozemulator-x86',
                   'testing/config/tooltool-manifests/androidx86/releng.manifest',
                   ['-debug',
                    'init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket',
                    '-qemu', '-m', '1024', '-enable-kvm'],
                   5554,
                   True,
                   20701, 20700)
}

def verify_android_device(build_obj, install=False):
    """
       Determine if any Android device is connected via adb.
       If no device is found, prompt to start an emulator.
       If a device is found or an emulator started and 'install' is
       specified, also check whether Firefox is installed on the
       device; if not, prompt to install Firefox.
       Returns True if the emulator was started or another device was
       already connected.
    """
    device_verified = False
    emulator = AndroidEmulator('*', substs=build_obj.substs)
    devices = emulator.dm.devices()
    if len(devices) > 0:
        device_verified = True
    elif emulator.is_available():
        response = raw_input(
            "No Android devices connected. Start an emulator? (Y/n) ").strip()
        if response.lower().startswith('y') or response == '':
            if not emulator.check_avd():
                print("Fetching AVD. This may take a while...")
                emulator.update_avd()
            print("Starting emulator running %s..." %
                emulator.get_avd_description())
            emulator.start()
            emulator.wait_for_start()
            device_verified = True

    if device_verified and install:
        # Determine if Firefox is installed on the device; if not,
        # prompt to install. This feature allows a test command to
        # launch an emulator, install Firefox, and proceed with testing
        # in one operation. It is also a basic safeguard against other
        # cases where testing is requested but Firefox installation has
        # been forgotten.
        # If Firefox is installed, there is no way to determine whether
        # the current build is installed, and certainly no way to
        # determine if the installed build is the desired build.
        # Installing every time is problematic because:
        #  - it prevents testing against other builds (downloaded apk)
        #  - installation may take a couple of minutes.
        installed = emulator.dm.shellCheckOutput(['pm', 'list',
            'packages', 'org.mozilla.'])
        if not 'fennec' in installed and not 'firefox' in installed:
            response = raw_input(
                "It looks like Firefox is not installed on this device.\n"
                "Install Firefox? (Y/n) ").strip()
            if response.lower().startswith('y') or response == '':
                print("Installing Firefox. This may take a while...")
                build_obj._run_make(directory=".", target='install',
                    ensure_exit_code=False)
    return device_verified


class AndroidEmulator(object):

    """
        Support running the Android emulator with an AVD from Mozilla
        test automation.

        Example usage:
            emulator = AndroidEmulator()
            if not emulator.is_running() and emulator.is_available():
                if not emulator.check_avd():
                    warn("this may take a while...")
                    emulator.update_avd()
                emulator.start()
                emulator.wait_for_start()
                emulator.wait()
    """

    def __init__(self, avd_type='4.3', verbose=False, substs=None):
        self.emulator_log = None
        self.emulator_path = 'emulator'
        self.verbose = verbose
        self.substs = substs
        self.avd_type = self._get_avd_type(avd_type)
        self.avd_info = AVD_DICT[self.avd_type]
        adb_path = self._find_sdk_exe('adb', False)
        if not adb_path:
            adb_path = 'adb'
        self.dm = DeviceManagerADB(autoconnect=False, adbPath=adb_path, retryLimit=1)
        self.dm.default_timeout = 10
        self._log_debug("Emulator created with type %s" % self.avd_type)

    def __del__(self):
        if self.emulator_log:
            self.emulator_log.close()

    def is_running(self):
        """
           Returns True if the Android emulator is running.
        """
        for proc in psutil.process_iter():
            name = proc.name()
            # On some platforms, "emulator" may start an emulator with
            # process name "emulator64-arm" or similar.
            if name and name.startswith('emulator'):
                return True
        return False

    def is_available(self):
        """
           Returns True if an emulator executable is found.
        """
        found = False
        emulator_path = self._find_sdk_exe('emulator', True)
        if emulator_path:
            self.emulator_path = emulator_path
            found = True
        return found

    def check_avd(self, force=False):
        """
           Determine if the AVD is already installed locally.
           (This is usually used to determine if update_avd() is likely
           to require a download; it is a convenient way of determining
           whether a 'this may take a while' warning is warranted.)

           Returns True if the AVD is installed.
        """
        avd = os.path.join(
            EMULATOR_HOME_DIR, 'avd', self.avd_info.name + '.avd')
        if force and os.path.exists(avd):
            shutil.rmtree(avd)
        if os.path.exists(avd):
            self._log_debug("AVD found at %s" % avd)
            return True
        return False

    def update_avd(self, force=False):
        """
           If required, update the AVD via tooltool.

           If the AVD directory is not found, or "force" is requested,
           download the tooltool manifest associated with the AVD and then
           invoke tooltool.py on the manifest. tooltool.py will download the
           required archive (unless already present in the local tooltool
           cache) and install the AVD.
        """
        avd = os.path.join(
            EMULATOR_HOME_DIR, 'avd', self.avd_info.name + '.avd')
        if force and os.path.exists(avd):
            shutil.rmtree(avd)
        if not os.path.exists(avd):
            self._fetch_tooltool()
            self._fetch_tooltool_manifest()
            self._tooltool_fetch()
            self._update_avd_paths()

    def start(self):
        """
           Launch the emulator.
        """
        def outputHandler(line):
            self.emulator_log.write("<%s>\n" % line)
        env = os.environ
        env['ANDROID_AVD_HOME'] = os.path.join(EMULATOR_HOME_DIR, "avd")
        command = [self.emulator_path, "-avd",
                   self.avd_info.name, "-port", "5554"]
        if self.avd_info.extra_args:
            command += self.avd_info.extra_args
        log_path = os.path.join(EMULATOR_HOME_DIR, 'emulator.log')
        self.emulator_log = open(log_path, 'w')
        self._log_debug("Starting the emulator with this command: %s" %
                        ' '.join(command))
        self._log_debug("Emulator output will be written to '%s'" %
                        log_path)
        self.proc = ProcessHandler(
            command, storeOutput=False, processOutputLine=outputHandler,
            env=env)
        self.proc.run()
        self._log_debug("Emulator started with pid %d" %
                        int(self.proc.proc.pid))

    def wait_for_start(self):
        """
           Verify that the emulator is running, the emulator device is visible
           to adb, and Android has booted.
        """
        if not self.proc:
            self._log_warning("Emulator not started!")
            return False
        if self.proc.proc.poll() is not None:
            self._log_warning("Emulator has already completed!")
            return False
        self._log_debug("Waiting for device status...")
        while(('emulator-5554', 'device') not in self.dm.devices()):
            time.sleep(10)
            if self.proc.proc.poll() is not None:
                self._log_warning("Emulator has already completed!")
                return False
        self._log_debug("Device status verified.")

        self._log_debug("Checking that Android has booted...")
        complete = False
        while(not complete):
            output = ''
            try:
                output = self.dm.shellCheckOutput(
                    ['getprop', 'sys.boot_completed'], timeout=5)
            except DMError:
                # adb not yet responding...keep trying
                pass
            if output.strip() == '1':
                complete = True
            else:
                time.sleep(10)
                if self.proc.proc.poll() is not None:
                    self._log_warning("Emulator has already completed!")
                    return False
        self._log_debug("Android boot status verified.")

        if not self._verify_emulator():
            return False
        if self.avd_info.uses_sut:
            if not self._verify_sut():
                return False
        return True

    def wait(self):
        """
           Wait for the emulator to close. If interrupted, close the emulator.
        """
        try:
            self.proc.wait()
        except:
            if self.proc.poll() is None:
                self.cleanup()
        return self.proc.poll()

    def cleanup(self):
        """
           Close the emulator.
        """
        self.proc.kill(signal.SIGTERM)

    def get_avd_description(self):
        """
           Return the human-friendly description of this AVD.
        """
        return self.avd_info.description

    def _log_debug(self, text):
        if self.verbose:
            print "DEBUG: %s" % text

    def _log_warning(self, text):
        print "WARNING: %s" % text

    def _fetch_tooltool(self):
        self._download_file(TOOLTOOL_URL, 'tooltool.py', EMULATOR_HOME_DIR)

    def _fetch_tooltool_manifest(self):
        url = 'https://hg.mozilla.org/%s/raw-file/%s/%s' % (
            "try", "default", self.avd_info.tooltool_manifest)
        self._download_file(url, 'releng.manifest', EMULATOR_HOME_DIR)

    def _tooltool_fetch(self):
        def outputHandler(line):
            self._log_debug(line)
        command = ['python', 'tooltool.py', 'fetch', '-m', 'releng.manifest']
        proc = ProcessHandler(
            command, processOutputLine=outputHandler, storeOutput=False,
            cwd=EMULATOR_HOME_DIR)
        proc.run()
        try:
            proc.wait()
        except:
            if proc.poll() is None:
                proc.kill(signal.SIGTERM)

    def _update_avd_paths(self):
        avd_path = os.path.join(EMULATOR_HOME_DIR, "avd")
        ini_file = os.path.join(avd_path, "test-1.ini")
        ini_file_new = os.path.join(avd_path, self.avd_info.name + ".ini")
        os.rename(ini_file, ini_file_new)
        avd_dir = os.path.join(avd_path, "test-1.avd")
        avd_dir_new = os.path.join(avd_path, self.avd_info.name + ".avd")
        os.rename(avd_dir, avd_dir_new)
        self._replace_ini_contents(ini_file_new)

    def _download_file(self, url, filename, path):
        f = urllib2.urlopen(url)
        if not os.path.isdir(path):
            try:
                os.makedirs(path)
            except Exception, e:
                self._log_warning(str(e))
                return False
        local_file = open(os.path.join(path, filename), 'wb')
        local_file.write(f.read())
        local_file.close()
        self._log_debug("Downloaded %s to %s/%s" % (url, path, filename))
        return True

    def _replace_ini_contents(self, path):
        with open(path, "r") as f:
            lines = f.readlines()
        with open(path, "w") as f:
            for line in lines:
                if line.startswith('path='):
                    avd_path = os.path.join(EMULATOR_HOME_DIR, "avd")
                    f.write('path=%s/%s.avd\n' %
                            (avd_path, self.avd_info.name))
                elif line.startswith('path.rel='):
                    f.write('path.rel=avd/%s.avd\n' % self.avd_info.name)
                else:
                    f.write(line)

    def _telnet_cmd(self, telnet, command):
        self._log_debug(">>> " + command)
        telnet.write('%s\n' % command)
        result = telnet.read_until('OK', 10)
        self._log_debug("<<< " + result)
        return result

    def _verify_emulator(self):
        telnet_ok = False
        tn = None
        while(not telnet_ok):
            try:
                tn = telnetlib.Telnet('localhost', self.avd_info.port, 10)
                if tn is not None:
                    res = tn.read_until('OK', 10)
                    self._telnet_cmd(tn, 'avd status')
                    if self.avd_info.uses_sut:
                        cmd = 'redir add tcp:%s:%s' % \
                           (str(self.avd_info.sut_port),
                            str(self.avd_info.sut_port))
                        self._telnet_cmd(tn, cmd)
                        cmd = 'redir add tcp:%s:%s' % \
                            (str(self.avd_info.sut_port2),
                             str(self.avd_info.sut_port2))
                        self._telnet_cmd(tn, cmd)
                    self._telnet_cmd(tn, 'redir list')
                    self._telnet_cmd(tn, 'network status')
                    tn.write('quit\n')
                    tn.read_all()
                    telnet_ok = True
                else:
                    self._log_warning("Unable to connect to port %d" % port)
            except:
                self._log_warning("Trying again after unexpected exception")
            finally:
                if tn is not None:
                    tn.close()
            if not telnet_ok:
                time.sleep(10)
                if self.proc.proc.poll() is not None:
                    self._log_warning("Emulator has already completed!")
                    return False
        return telnet_ok

    def _verify_sut(self):
        sut_ok = False
        while(not sut_ok):
            try:
                tn = telnetlib.Telnet('localhost', self.avd_info.sut_port, 10)
                if tn is not None:
                    self._log_debug(
                        "Connected to port %d" % self.avd_info.sut_port)
                    res = tn.read_until('$>', 10)
                    if res.find('$>') == -1:
                        self._log_debug("Unexpected SUT response: %s" % res)
                    else:
                        self._log_debug("SUT response: %s" % res)
                        sut_ok = True
                    tn.write('quit\n')
                    tn.read_all()
            except:
                self._log_debug("Caught exception while verifying sutagent")
            finally:
                if tn is not None:
                    tn.close()
            if not sut_ok:
                time.sleep(10)
                if self.proc.proc.poll() is not None:
                    self._log_warning("Emulator has already completed!")
                    return False
        return sut_ok

    def _get_avd_type(self, requested):
        if requested in AVD_DICT.keys():
            return requested
        if self.substs:
            if not self.substs['TARGET_CPU'].startswith('arm'):
                return 'x86'
            if self.substs['MOZ_ANDROID_MIN_SDK_VERSION'] == '9':
                return '2.3'
        return '4.3'

    def _find_sdk_exe(self, exe, tools):
        if tools:
            subdir = 'tools'
            var = 'ANDROID_TOOLS'
        else:
            subdir = 'platform-tools'
            var = 'ANDROID_PLATFORM_TOOLS'

        found = False
        # Is exe on PATH?
        exe_path = find_executable(exe)
        if exe_path:
            found = True
        else:
            self._log_debug("Unable to find executable on PATH")

        if not found:
            # Can exe be found in the Android SDK?
            try:
                android_sdk_root = os.environ['ANDROID_SDK_ROOT']
                exe_path = os.path.join(
                    android_sdk_root, subdir, exe)
                if os.path.exists(exe_path):
                    found = True
                else:
                    self._log_debug(
                        "Unable to find executable at %s" % exe_path)
            except KeyError:
                self._log_debug("ANDROID_SDK_ROOT not set")

        if not found and self.substs:
            # Can exe be found in ANDROID_TOOLS/ANDROID_PLATFORM_TOOLS?
            try:
                exe_path = os.path.join(
                    self.substs[var], exe)
                if os.path.exists(exe_path):
                    found = True
                else:
                    self._log_debug(
                        "Unable to find executable at %s" % exe_path)
            except KeyError:
                self._log_debug("%s not set" % var)

        if not found:
            # Can exe be found in the default bootstrap location?
            exe_path = os.path.join(
                '~', '.mozbuild', 'android-sdk-linux', subdir, exe)
            if os.path.exists(exe_path):
                found = True
            else:
                self._log_debug(
                    "Unable to find executable at %s" % exe_path)

        if found:
            self._log_debug("%s found at %s" % (exe, exe_path))
        else:
            exe_path = None
        return exe_path
