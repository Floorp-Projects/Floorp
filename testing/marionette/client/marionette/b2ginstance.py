# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from ConfigParser import ConfigParser
import os
import platform
import posixpath
import shutil
import subprocess
import sys
import tempfile
import traceback

from mozdevice import DeviceManagerADB
import mozcrash

class B2GInstance(object):
    @classmethod
    def check_b2g_dir(cls, dir):
        if os.path.isfile(os.path.join(dir, 'load-config.sh')):
            return dir

        oldstyle_dir = os.path.join(dir, 'glue', 'gonk-ics')
        if os.access(oldstyle_dir, os.F_OK):
            return oldstyle_dir

        return None

    @classmethod
    def find_b2g_dir(cls):
        for env_var in ('B2G_DIR', 'B2G_HOME'):
            if env_var in os.environ:
                env_dir = os.environ[env_var]
                env_dir = cls.check_b2g_dir(env_dir)
                if env_dir:
                    return env_dir

        cwd = os.getcwd()
        cwd = cls.check_b2g_dir(cwd)
        if cwd:
            return cwd

        return None

    @classmethod
    def check_adb(cls, homedir):
        if 'ADB' in os.environ:
            env_adb = os.environ['ADB']
            if os.path.exists(env_adb):
                return env_adb

        return cls.check_host_binary(homedir, 'adb')

    @classmethod
    def check_fastboot(cls, homedir):
        return cls.check_host_binary(homedir, 'fastboot')

    @classmethod
    def check_host_binary(cls, homedir, binary):
        host_dir = "linux-x86"
        if platform.system() == "Darwin":
            host_dir = "darwin-x86"
        binary_path = subprocess.Popen(['which', binary],
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT)
        if binary_path.wait() == 0:
            return binary_path.stdout.read().strip()  # remove trailing newline
        binary_paths = [os.path.join(homedir, 'glue', 'gonk', 'out', 'host',
                                     host_dir, 'bin', binary),
                        os.path.join(homedir, 'out', 'host', host_dir,
                                     'bin', binary),
                        os.path.join(homedir, 'bin', binary)]
        for option in binary_paths:
            if os.path.exists(option):
                return option
        raise Exception('%s not found!' % binary)

    def __init__(self, homedir=None, devicemanager=None, emulator=False):
        if not homedir:
            homedir = self.find_b2g_dir()
        else:
            homedir = self.check_b2g_dir(homedir)

        if not homedir:
            raise EnvironmentError('Must define B2G_HOME or pass the homedir parameter')

        self.homedir = homedir
        self.adb_path = self.check_adb(self.homedir)
        self.fastboot_path = None if emulator else self.check_fastboot(self.homedir)
        self.update_tools = os.path.join(self.homedir, 'tools', 'update-tools')
        self._dm = devicemanager
        self._remote_profiles = None

    @property
    def dm(self):
        if not self._dm:
            self._dm = DeviceManagerADB(adbPath=self.adb_path)
        return self._dm

    @property
    def remote_profiles(self):
        if not self._remote_profiles:
            self.check_remote_profiles()
        return self._remote_profiles

    def check_file(self, filePath):
        if not os.access(filePath, os.F_OK):
            raise Exception(('File not found: %s; did you pass the B2G home '
                             'directory as the homedir parameter, or set '
                             'B2G_HOME correctly?') % filePath)

    def check_remote_profiles(self, remote_profiles_ini='/data/b2g/mozilla/profiles.ini'):
        if not self.dm.fileExists(remote_profiles_ini):
            raise Exception("Remote file '%s' not found" % remote_profiles_ini)

        local_profiles_ini = tempfile.NamedTemporaryFile()
        self.dm.getFile(remote_profiles_ini, local_profiles_ini.name)
        cfg = ConfigParser()
        cfg.read(local_profiles_ini.name)

        remote_profiles = []
        for section in cfg.sections():
            if cfg.has_option(section, 'Path'):
                if cfg.has_option(section, 'IsRelative') and cfg.getint(section, 'IsRelative'):
                    remote_profiles.append(posixpath.join(posixpath.dirname(remote_profiles_ini), cfg.get(section, 'Path')))
                else:
                    remote_profiles.append(cfg.get(section, 'Path'))
        self._remote_profiles = remote_profiles
        return remote_profiles

    def check_for_crashes(self, symbols_path):
        remote_dump_dirs = [posixpath.join(p, 'minidumps') for p in self.remote_profiles]
        crashed = False
        for remote_dump_dir in remote_dump_dirs:
            local_dump_dir = tempfile.mkdtemp()
            self.dm.getDirectory(remote_dump_dir, local_dump_dir)
            try:
                if mozcrash.check_for_crashes(local_dump_dir, symbols_path):
                    crashed = True
            except:
                traceback.print_exc()
            finally:
                shutil.rmtree(local_dump_dir)
                self.dm.removeDir(remote_dump_dir)
        return crashed

    def import_update_tools(self):
        """Import the update_tools package from B2G"""
        sys.path.append(self.update_tools)
        import update_tools
        sys.path.pop()
        return update_tools
