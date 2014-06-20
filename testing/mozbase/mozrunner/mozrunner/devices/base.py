from ConfigParser import (
    ConfigParser,
    RawConfigParser
)
import datetime
import os
import posixpath
import socket
import subprocess
import tempfile
import time
import traceback

from mozdevice import DMError

class Device(object):
    def __init__(self, app_ctx, restore=True):
        self.app_ctx = app_ctx
        self.dm = self.app_ctx.dm
        self.restore = restore
        self.added_files = set()
        self.backup_files = set()

    @property
    def remote_profiles(self):
        """
        A list of remote profiles on the device.
        """
        remote_ini = self.app_ctx.remote_profiles_ini
        if not self.dm.fileExists(remote_ini):
            raise Exception("Remote file '%s' not found" % remote_ini)

        local_ini = tempfile.NamedTemporaryFile()
        self.dm.getFile(remote_ini, local_ini.name)
        cfg = ConfigParser()
        cfg.read(local_ini.name)

        profiles = []
        for section in cfg.sections():
            if cfg.has_option(section, 'Path'):
                if cfg.has_option(section, 'IsRelative') and cfg.getint(section, 'IsRelative'):
                    profiles.append(posixpath.join(posixpath.dirname(remote_ini), \
                                    cfg.get(section, 'Path')))
                else:
                    profiles.append(cfg.get(section, 'Path'))
        return profiles

    def pull_minidumps(self):
        """
        Saves any minidumps found in the remote profile on the local filesystem.

        :returns: Path to directory containing the dumps.
        """
        remote_dump_dir = posixpath.join(self.app_ctx.remote_profile, 'minidumps')
        local_dump_dir = tempfile.mkdtemp()
        self.dm.getDirectory(remote_dump_dir, local_dump_dir)
        self.dm.removeDir(remote_dump_dir)
        return local_dump_dir

    def setup_profile(self, profile):
        """
        Copy profile to the device and update the remote profiles.ini
        to point to the new profile.

        :param profile: mozprofile object to copy over.
        """
        self.dm.remount()

        if self.dm.dirExists(self.app_ctx.remote_profile):
            self.dm.shellCheckOutput(['rm', '-r', self.app_ctx.remote_profile])

        self.dm.pushDir(profile.profile, self.app_ctx.remote_profile)

        extension_dir = os.path.join(profile.profile, 'extensions', 'staged')
        if os.path.isdir(extension_dir):
            # Copy the extensions to the B2G bundles dir.
            # need to write to read-only dir
            for filename in os.listdir(extension_dir):
                path = posixpath.join(self.app_ctx.remote_bundles_dir, filename)
                if self.dm.fileExists(path):
                    self.dm.shellCheckOutput(['rm', '-rf', path])
            self.dm.pushDir(extension_dir, self.app_ctx.remote_bundles_dir)

        timeout = 5 # seconds
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            if self.dm.fileExists(self.app_ctx.remote_profiles_ini):
                break
            time.sleep(1)
        else:
            print "timed out waiting for profiles.ini"

        local_profiles_ini = tempfile.NamedTemporaryFile()
        self.dm.getFile(self.app_ctx.remote_profiles_ini, local_profiles_ini.name)

        config = ProfileConfigParser()
        config.read(local_profiles_ini.name)
        for section in config.sections():
            if 'Profile' in section:
                config.set(section, 'IsRelative', 0)
                config.set(section, 'Path', self.app_ctx.remote_profile)

        new_profiles_ini = tempfile.NamedTemporaryFile()
        config.write(open(new_profiles_ini.name, 'w'))

        self.backup_file(self.app_ctx.remote_profiles_ini)
        self.dm.pushFile(new_profiles_ini.name, self.app_ctx.remote_profiles_ini)

    def install_busybox(self, busybox):
        """
        Installs busybox on the device.

        :param busybox: Path to busybox binary to install.
        """
        self.dm.remount()
        print 'pushing %s' % self.app_ctx.remote_busybox
        self.dm.pushFile(busybox, self.app_ctx.remote_busybox, retryLimit=10)
        # TODO for some reason using dm.shellCheckOutput doesn't work,
        #      while calling adb shell directly does.
        args = [self.app_ctx.adb, '-s', self.dm._deviceSerial,
                'shell', 'cd /system/bin; chmod 555 busybox;' \
                'for x in `./busybox --list`; do ln -s ./busybox $x; done']
        adb = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        adb.wait()
        self.dm._verifyZip()

    def setup_port_forwarding(self, remote_port):
        """
        Set up TCP port forwarding to the specified port on the device,
        using any availble local port, and return the local port.

        :param remote_port: The remote port to wait on.
        """
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(("",0))
        local_port = s.getsockname()[1]
        s.close()

        self.dm.forward('tcp:%d' % local_port, 'tcp:%d' % remote_port)
        return local_port

    def wait_for_port(self, port, timeout=300):
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect(('localhost', port))
                data = sock.recv(16)
                sock.close()
                if ':' in data:
                    return True
            except:
                traceback.print_exc()
            time.sleep(1)
        return False

    def backup_file(self, remote_path):
        if not self.restore:
            return

        if self.dm.fileExists(remote_path):
            self.dm.copyTree(remote_path, '%s.orig' % remote_path)
            self.backup_files.add(remote_path)
        else:
            self.added_files.add(remote_path)

    def cleanup(self):
        """
        Cleanup the device.
        """
        if not self.restore:
            return

        try:
            self.dm._verifyDevice()
        except DMError:
            return

        self.dm.remount()
        # Restore the original profile
        for added_file in self.added_files:
            self.dm.removeFile(added_file)

        for backup_file in self.backup_files:
            if self.dm.fileExists('%s.orig' % backup_file):
                self.dm.moveTree('%s.orig' % backup_file, backup_file)

        # Delete any bundled extensions
        extension_dir = posixpath.join(self.app_ctx.remote_profile, 'extensions', 'staged')
        if self.dm.dirExists(extension_dir):
            for filename in self.dm.listFiles(extension_dir):
                try:
                    self.dm.removeDir(posixpath.join(self.app_ctx.remote_bundles_dir, filename))
                except DMError:
                    pass
        # Remove the test profile
        self.dm.removeDir(self.app_ctx.remote_profile)


class ProfileConfigParser(RawConfigParser):
    """
    Class to create profiles.ini config files

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

