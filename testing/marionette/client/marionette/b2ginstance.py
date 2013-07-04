# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from ConfigParser import ConfigParser
import posixpath
import shutil
import tempfile
import traceback

from b2gbuild import B2GBuild
from mozdevice import DeviceManagerADB
import mozcrash


class B2GInstance(B2GBuild):

    def __init__(self, devicemanager=None, **kwargs):
        B2GBuild.__init__(self, **kwargs)

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
