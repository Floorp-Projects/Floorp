import ConfigParser
import os
import posixpath
import re
import shutil
import subprocess
import tempfile
import time
import traceback

from runner import Runner
from mozdevice import DMError
import mozcrash
import mozlog

__all__ = ['RemoteRunner', 'B2GRunner', 'remote_runners']

class RemoteRunner(Runner):

    def __init__(self, profile,
                       devicemanager,
                       clean_profile=None,
                       process_class=None,
                       env=None,
                       remote_test_root=None,
                       restore=True):

        super(RemoteRunner, self).__init__(profile, clean_profile=clean_profile,
                                                process_class=process_class, env=env)
        self.log = mozlog.getLogger('RemoteRunner')

        self.dm = devicemanager
        self.remote_test_root = remote_test_root or self.dm.getDeviceRoot()
        self.remote_profile = posixpath.join(self.remote_test_root, 'profile')
        self.restore = restore
        self.backup_files = set([])

    def backup_file(self, remote_path):
        if not self.restore:
            return

        if self.dm.fileExists(remote_path):
            self.dm.shellCheckOutput(['dd', 'if=%s' % remote_path, 'of=%s.orig' % remote_path])
            self.backup_files.add(remote_path)


    def check_for_crashes(self, symbols_path, last_test=None):
        crashed = False
        remote_dump_dir = posixpath.join(self.remote_profile, 'minidumps')
        self.log.info("checking for crashes in '%s'" % remote_dump_dir)
        if self.dm.dirExists(remote_dump_dir):
            local_dump_dir = tempfile.mkdtemp()
            self.dm.getDirectory(remote_dump_dir, local_dump_dir)
            try:
                crashed = mozcrash.check_for_crashes(local_dump_dir, symbols_path, test_name=last_test)
            except:
                traceback.print_exc()
            finally:
                shutil.rmtree(local_dump_dir)
                self.dm.removeDir(remote_dump_dir)
        return crashed

    def cleanup(self):
        if not self.restore:
            return

        super(RemoteRunner, self).cleanup()

        for backup_file in self.backup_files:
            # Restore the original profiles.ini
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

    def __init__(self, profile, devicemanager, marionette, context_chrome=True,
                 test_script=None, test_script_args=None, **kwargs):

        super(B2GRunner, self).__init__(profile, devicemanager, **kwargs)
        self.log = mozlog.getLogger('B2GRunner')

        tmpfd, processLog = tempfile.mkstemp(suffix='pidlog')
        os.close(tmpfd)
        tmp_env = self.env or {}
        self.env = { 'MOZ_CRASHREPORTER': '1',
                     'MOZ_CRASHREPORTER_NO_REPORT': '1',
                     'MOZ_HIDE_RESULTS_TABLE': '1',
                     'MOZ_PROCESS_LOG': processLog,
                     'NO_EM_RESTART': '1', }
        self.env.update(tmp_env)
        self.last_test = "automation"
        self.marionette = marionette
        self.context_chrome = context_chrome
        self.test_script = test_script
        self.test_script_args = test_script_args
        self.remote_profiles_ini = '/data/b2g/mozilla/profiles.ini'
        self.bundles_dir = '/system/b2g/distribution/bundles'
        self.user_js = '/data/local/user.js'

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
        if not self.marionette.emulator:
            self._reboot_device()
            #wait for wlan to come up
            if not self._wait_for_net():
                raise Exception("network did not come up, please configure the network" +
                                " prior to running before running the automation framework")

        self.dm.shellCheckOutput(['stop', 'b2g'])

        self.kp_kwargs['processOutputLine'] = [self.on_output]
        self.kp_kwargs['onTimeout'] = [self.on_timeout]
        self.process_handler = self.process_class(self.command, **self.kp_kwargs)
        self.process_handler.run(timeout=timeout, outputTimeout=outputTimeout)

        # Set up port forwarding again for Marionette, since any that
        # existed previously got wiped out by the reboot.
        if not self.marionette.emulator:
            subprocess.Popen([self.dm._adbPath,
                              'forward',
                              'tcp:%s' % self.marionette.port,
                              'tcp:%s' % self.marionette.port]).communicate()
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

        # run the script that starts the tests

        if self.test_script:
            if os.path.isfile(self.test_script):
                script = open(self.test_script, 'r')
                self.marionette.execute_script(script.read(), script_args=self.test_script_args)
                script.close()
            elif isinstance(self.test_script, basestring):
                self.marionette.execute_script(self.test_script, script_args=self.test_script_args)
        else:
            # assumes the tests are started on startup automatically
            pass

    def on_output(self, line):
        print line
        match = re.findall(r"TEST-START \| ([^\s]*)", line)
        if match:
            self.last_test = match[-1]

    def on_timeout(self):
        self.log.testFail("%s | application timed "
                         "out after %s seconds with no output",
                         self.last_test, self.timeout)

    def _reboot_device(self):
        serial, status = self._get_device_status()
        self.dm.shellCheckOutput(['/system/bin/reboot'])

        # The reboot command can return while adb still thinks the device is
        # connected, so wait a little bit for it to disconnect from adb.
        time.sleep(10)

        # wait for device to come back to previous status
        self.log.info('waiting for device to come back online after reboot')
        start = time.time()
        rserial, rstatus = self._get_device_status(serial)
        while rstatus != 'device':
            if time.time() - start > 120:
                # device hasn't come back online in 2 minutes, something's wrong
                raise Exception("Device %s (status: %s) not back online after reboot" % (serial, rstatus))
            time.sleep(5)
            rserial, rstatus = self.getDeviceStatus(serial)
        self.log.info('device:', serial, 'status:', rstatus)

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
            proc = subprocess.Popen([self.dm._adbPath, 'shell', '/system/bin/netcfg'])
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
        """
        Copy profile and update the remote profiles ini file to point to the new profile
        """

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
            subprocess.Popen([self.dm._adbPath, 'remount']).communicate()
            for filename in os.listdir(extension_dir):
                self.dm.shellCheckOutput(['rm', '-rf',
                                          os.path.join(self.bundles_dir, filename)])
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

        # In B2G, user.js is always read from /data/local, not the profile
        # directory.  Backup the original user.js first so we can restore it.
        self.backup_file(self.user_js)
        self.dm.pushFile(os.path.join(self.profile.profile, "user.js"), self.user_js)

    def cleanup(self):
        super(B2GRunner, self).cleanup()
        if getattr(self.marionette, 'instance', False):
            self.marionette.instance.close()
        del self.marionette

class ProfileConfigParser(ConfigParser.RawConfigParser):
    """Subclass of RawConfigParser that outputs .ini files in the exact
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
