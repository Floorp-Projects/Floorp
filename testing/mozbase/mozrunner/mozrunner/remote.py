# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import ConfigParser
import os
import posixpath
import re
import signal
from StringIO import StringIO
import subprocess
import sys
import tempfile
import time

from mozdevice import DMError
import mozfile
import mozlog

from .base import Runner


__all__ = ['B2GRunner',
           'RemoteRunner',
           'remote_runners']


class RemoteRunner(Runner):

    def __init__(self, profile,
                       devicemanager,
                       clean_profile=None,
                       process_class=None,
                       env=None,
                       remote_test_root=None,
                       restore=True,
                       **kwargs):

        Runner.__init__(self, profile, clean_profile=clean_profile,
                        process_class=process_class, env=env, **kwargs)
        self.log = mozlog.getLogger('RemoteRunner')

        self.dm = devicemanager
        self.last_test = None
        self.remote_test_root = remote_test_root or self.dm.getDeviceRoot()
        self.log.info('using %s as test_root' % self.remote_test_root)
        self.remote_profile = posixpath.join(self.remote_test_root, 'profile')
        self.restore = restore
        self.added_files = set()
        self.backup_files = set()

    def backup_file(self, remote_path):
        if not self.restore:
            return

        if self.dm.fileExists(remote_path):
            self.dm.shellCheckOutput(['dd', 'if=%s' % remote_path, 'of=%s.orig' % remote_path])
            self.backup_files.add(remote_path)
        else:
            self.added_files.add(remote_path)

    def check_for_crashes(self, last_test=None):
        last_test = last_test or self.last_test
        remote_dump_dir = posixpath.join(self.remote_profile, 'minidumps')
        crashed = False

        self.log.info("checking for crashes in '%s'" % remote_dump_dir)
        if self.dm.dirExists(remote_dump_dir):
            local_dump_dir = tempfile.mkdtemp()
            self.dm.getDirectory(remote_dump_dir, local_dump_dir)

            crashed = Runner.check_for_crashes(self, local_dump_dir, \
                                               test_name=last_test)
            mozfile.remove(local_dump_dir)
            self.dm.removeDir(remote_dump_dir)

        return crashed

    def cleanup(self):
        if not self.restore:
            return

        Runner.cleanup(self)

        self.dm.remount()
        # Restore the original profile
        for added_file in self.added_files:
            self.dm.removeFile(added_file)

        for backup_file in self.backup_files:
            if self.dm.fileExists('%s.orig' % backup_file):
                self.dm.shellCheckOutput(['dd', 'if=%s.orig' % backup_file, 'of=%s' % backup_file])
                self.dm.removeFile("%s.orig" % backup_file)

        # Delete any bundled extensions
        extension_dir = posixpath.join(self.remote_profile, 'extensions', 'staged')
        if self.dm.dirExists(extension_dir):
            for filename in self.dm.listFiles(extension_dir):
                try:
                    self.dm.removeDir(posixpath.join(self.bundles_dir, filename))
                except DMError:
                    pass
        # Remove the test profile
        self.dm.removeDir(self.remote_profile)


class B2GRunner(RemoteRunner):

    def __init__(self, profile, devicemanager, marionette=None, context_chrome=True,
                 test_script=None, test_script_args=None,
                 marionette_port=None, emulator=None, **kwargs):

        remote_test_root = kwargs.get('remote_test_root')
        if not remote_test_root:
            kwargs['remote_test_root'] = '/data/local'
        RemoteRunner.__init__(self, profile, devicemanager, **kwargs)
        self.log = mozlog.getLogger('B2GRunner')

        tmpfd, processLog = tempfile.mkstemp(suffix='pidlog')
        os.close(tmpfd)
        tmp_env = self.env or {}
        self.env = { 'MOZ_CRASHREPORTER': '1',
                     'MOZ_CRASHREPORTER_NO_REPORT': '1',
                     'MOZ_HIDE_RESULTS_TABLE': '1',
                     'MOZ_PROCESS_LOG': processLog,
                     'NSPR_LOG_MODULES': 'signaling:5,mtransport:3',
                     'R_LOG_LEVEL': '5',
                     'R_LOG_DESTINATION': 'stderr',
                     'R_LOG_VERBOSE': '1',
                     'NO_EM_RESTART': '1', }
        self.env.update(tmp_env)
        self.last_test = "automation"

        self.marionette = marionette
        if self.marionette is not None:
            if marionette_port is None:
                marionette_port = self.marionette.port
            elif self.marionette.port != marionette_port:
                raise ValueError("Got a marionette object and a port but they don't match")

            if emulator is None:
                emulator = marionette.emulator
            elif marionette.emulator != emulator:
                raise ValueError("Got a marionette object and an emulator argument but they don't match")

        self.marionette_port = marionette_port
        self.emulator = emulator

        self.context_chrome = context_chrome
        self.test_script = test_script
        self.test_script_args = test_script_args
        self.remote_profiles_ini = '/data/b2g/mozilla/profiles.ini'
        self.bundles_dir = '/system/b2g/distribution/bundles'

    @property
    def command(self):
        cmd = [self.dm._adbPath]
        if self.dm._deviceSerial:
            cmd.extend(['-s', self.dm._deviceSerial])
        cmd.append('shell')
        for k, v in self.env.iteritems():
            cmd.append("%s=%s" % (k, v))
        cmd.append('/system/bin/b2g.sh')
        return cmd

    def start(self, timeout=None, outputTimeout=None):
        self.timeout = timeout
        self.outputTimeout = outputTimeout
        self._setup_remote_profile()
        # reboot device so it starts up with the proper profile
        if not self.emulator:
            self.dm.reboot(wait=True)
            #wait for wlan to come up
            if not self._wait_for_net():
                raise Exception("network did not come up, please configure the network" +
                                " prior to running before running the automation framework")

        self.dm.shellCheckOutput(['stop', 'b2g'])

        # For some reason user.js in the profile doesn't get picked up.
        # Manually copy it over to prefs.js. See bug 1009730 for more details.
        self.dm.moveTree(posixpath.join(self.remote_profile, 'user.js'),
                         posixpath.join(self.remote_profile, 'prefs.js'))

        self.kp_kwargs.update({'stream': sys.stdout,
                               'processOutputLine': self.on_output,
                               'onTimeout': self.on_timeout,})
        self.process_handler = self.process_class(self.command, **self.kp_kwargs)
        self.process_handler.run(timeout=timeout, outputTimeout=outputTimeout)

        # Set up port forwarding again for Marionette, since any that
        # existed previously got wiped out by the reboot.
        if self.emulator is None:
            subprocess.Popen([self.dm._adbPath,
                              'forward',
                              'tcp:%s' % self.marionette_port,
                              'tcp:2828']).communicate()

        if self.marionette is not None:
            self.start_marionette()

        if self.test_script is not None:
            self.start_tests()

    def start_marionette(self):
        self.marionette.wait_for_port()

        # start a marionette session
        session = self.marionette.start_session()
        if 'b2g' not in session:
            raise Exception("bad session value %s returned by start_session" % session)

        if self.marionette.emulator:
            # Disable offline status management (bug 777145), otherwise the network
            # will be 'offline' when the mochitests start.  Presumably, the network
            # won't be offline on a real device, so we only do this for emulators.
            self.marionette.set_context(self.marionette.CONTEXT_CHROME)
            self.marionette.execute_script("""
                Components.utils.import("resource://gre/modules/Services.jsm");
                Services.io.manageOfflineStatus = false;
                Services.io.offline = false;
                """)

        if self.context_chrome:
            self.marionette.set_context(self.marionette.CONTEXT_CHROME)
        else:
            self.marionette.set_context(self.marionette.CONTEXT_CONTENT)


    def start_tests(self):
        #self.marionette.execute_script("""
        #    var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
        #    var homeUrl = prefs.getCharPref("browser.homescreenURL");
        #    dump(homeURL + "\n");
        #""")

        # run the script that starts the tests
        if os.path.isfile(self.test_script):
            script = open(self.test_script, 'r')
            self.marionette.execute_script(script.read(), script_args=self.test_script_args)
            script.close()
        elif isinstance(self.test_script, basestring):
            self.marionette.execute_script(self.test_script, script_args=self.test_script_args)

    def on_output(self, line):
        match = re.findall(r"TEST-START \| ([^\s]*)", line)
        if match:
            self.last_test = match[-1]

    def on_timeout(self):
        self.dm.killProcess('/system/b2g/b2g', sig=signal.SIGABRT)

        msg = "%s | application timed out after %s seconds"

        if self.timeout:
            timeout = self.timeout
        else:
            timeout = self.outputTimeout
            msg = "%s with no output" % msg

        self.log.testFail(msg % (self.last_test, timeout))
        self.check_for_crashes()

    def _get_device_status(self, serial=None):
        # If we know the device serial number, we look for that,
        # otherwise we use the (presumably only) device shown in 'adb devices'.
        serial = serial or self.dm._deviceSerial
        status = 'unknown'

        proc = subprocess.Popen([self.dm._adbPath, 'devices'], stdout=subprocess.PIPE)
        line = proc.stdout.readline()
        while line != '':
            result = re.match('(.*?)\t(.*)', line)
            if result:
                thisSerial = result.group(1)
                if not serial or thisSerial == serial:
                    serial = thisSerial
                    status = result.group(2)
                    break
            line = proc.stdout.readline()
        return (serial, status)

    def _wait_for_net(self):
        active = False
        time_out = 0
        while not active and time_out < 40:
            proc = subprocess.Popen([self.dm._adbPath, 'shell', '/system/bin/netcfg'], stdout=subprocess.PIPE)
            proc.stdout.readline() # ignore first line
            line = proc.stdout.readline()
            while line != "":
                if (re.search(r'UP\s+(?:[0-9]{1,3}\.){3}[0-9]{1,3}', line)):
                    active = True
                    break
                line = proc.stdout.readline()
            time_out += 1
            time.sleep(1)
        return active

    def _setup_remote_profile(self):
        """Copy profile and update the remote profiles.ini to point to the new profile"""
        self.dm.remount()

        # copy the profile to the device.
        if self.dm.dirExists(self.remote_profile):
            self.dm.shellCheckOutput(['rm', '-r', self.remote_profile])

        try:
            self.dm.pushDir(self.profile.profile, self.remote_profile)
        except DMError:
            self.log.error("Automation Error: Unable to copy profile to device.")
            raise

        extension_dir = os.path.join(self.profile.profile, 'extensions', 'staged')
        if os.path.isdir(extension_dir):
            # Copy the extensions to the B2G bundles dir.
            # need to write to read-only dir
            for filename in os.listdir(extension_dir):
                fpath = os.path.join(self.bundles_dir, filename)
                if self.dm.fileExists(fpath):
                    self.dm.shellCheckOutput(['rm', '-rf', fpath])
            try:
                self.dm.pushDir(extension_dir, self.bundles_dir)
            except DMError:
                self.log.error("Automation Error: Unable to copy extensions to device.")
                raise

        if not self.dm.fileExists(self.remote_profiles_ini):
            raise DMError("The profiles.ini file '%s' does not exist on the device" % self.remote_profiles_ini)

        local_profiles_ini = tempfile.NamedTemporaryFile()
        self.dm.getFile(self.remote_profiles_ini, local_profiles_ini.name)

        config = ProfileConfigParser()
        config.read(local_profiles_ini.name)
        for section in config.sections():
            if 'Profile' in section:
                config.set(section, 'IsRelative', 0)
                config.set(section, 'Path', self.remote_profile)

        new_profiles_ini = tempfile.NamedTemporaryFile()
        config.write(open(new_profiles_ini.name, 'w'))

        self.backup_file(self.remote_profiles_ini)
        self.dm.pushFile(new_profiles_ini.name, self.remote_profiles_ini)

    def cleanup(self):
        RemoteRunner.cleanup(self)
        if getattr(self.marionette, 'instance', False):
            self.marionette.instance.close()
        del self.marionette


class ProfileConfigParser(ConfigParser.RawConfigParser):
    """Class to create profiles.ini config files

    Subclass of RawConfigParser that outputs .ini files in the exact
    format expected for profiles.ini, which is slightly different
    than the default format.

    """

    def optionxform(self, optionstr):
        return optionstr

    def write(self, fp):
        if self._defaults:
            fp.write("[%s]\n" % ConfigParser.DEFAULTSECT)
            for (key, value) in self._defaults.items():
                fp.write("%s=%s\n" % (key, str(value).replace('\n', '\n\t')))
            fp.write("\n")
        for section in self._sections:
            fp.write("[%s]\n" % section)
            for (key, value) in self._sections[section].items():
                if key == "__name__":
                    continue
                if (value is not None) or (self._optcre == self.OPTCRE):
                    key = "=".join((key, str(value).replace('\n', '\n\t')))
                fp.write("%s\n" % (key))
            fp.write("\n")

remote_runners = {'b2g': 'B2GRunner',
                  'fennec': 'FennecRunner'}
