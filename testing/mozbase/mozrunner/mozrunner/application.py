# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from distutils.spawn import find_executable
import glob
import os
import posixpath

from mozdevice import DeviceManagerADB, DMError
from mozprofile import (
    Profile,
    FirefoxProfile,
    MetroFirefoxProfile,
    ThunderbirdProfile
)

here = os.path.abspath(os.path.dirname(__file__))

def get_app_context(appname):
    context_map = { 'default': DefaultContext,
                    'b2g': B2GContext,
                    'firefox': FirefoxContext,
                    'thunderbird': ThunderbirdContext,
                    'metro': MetroContext }
    if appname not in context_map:
        raise KeyError("Application '%s' not supported!" % appname)
    return context_map[appname]


class DefaultContext(object):
    profile_class = Profile


class B2GContext(object):
    _bindir = None
    _dm = None
    _remote_profile = None
    _remote_settings_db = None
    profile_class = Profile

    def __init__(self, b2g_home=None, adb_path=None):
        self.homedir = b2g_home or os.environ.get('B2G_HOME')

        if self.homedir is not None and not os.path.isdir(self.homedir):
            raise OSError('Homedir \'%s\' does not exist!' % self.homedir)

        self._adb = adb_path
        self._update_tools = None
        self._fastboot = None

        self.remote_binary = '/system/bin/b2g.sh'
        self.remote_bundles_dir = '/system/b2g/distribution/bundles'
        self.remote_busybox = '/system/bin/busybox'
        self.remote_process = '/system/b2g/b2g'
        self.remote_profiles_ini = '/data/b2g/mozilla/profiles.ini'
        self.remote_settings_json = '/system/b2g/defaults/settings.json'
        self.remote_idb_dir = '/data/local/storage/persistent/chrome/idb'
        self.remote_test_root = '/data/local/tests'
        self.remote_webapps_dir = '/data/local/webapps'

        self.remote_backup_files = [
            self.remote_settings_json,
            self.remote_webapps_dir,
        ]

    @property
    def fastboot(self):
        if self._fastboot is None:
            self._fastboot = self.which('fastboot')
        return self._fastboot

    @property
    def update_tools(self):
        if self._update_tools is None:
            self._update_tools = os.path.join(self.homedir, 'tools', 'update-tools')
        return self._update_tools

    @property
    def adb(self):
        if not self._adb:
            paths = [os.environ.get('ADB'),
                     os.environ.get('ADB_PATH'),
                     self.which('adb')]
            paths = [p for p in paths if p is not None if os.path.isfile(p)]
            if not paths:
                raise OSError('Could not find the adb binary, make sure it is on your' \
                              'path or set the $ADB_PATH environment variable.')
            self._adb = paths[0]
        return self._adb

    @property
    def bindir(self):
        if self._bindir is None and self.homedir is not None:
            # TODO get this via build configuration
            path = os.path.join(self.homedir, 'out', 'host', '*', 'bin')
            self._bindir = glob.glob(path)[0]
        return self._bindir

    @property
    def dm(self):
        if not self._dm:
            self._dm = DeviceManagerADB(adbPath=self.adb, autoconnect=False, deviceRoot=self.remote_test_root)
        return self._dm

    @property
    def remote_profile(self):
        if not self._remote_profile:
            self._remote_profile = posixpath.join(self.remote_test_root, 'profile')
        return self._remote_profile

    @property
    def remote_settings_db(self):
        if not self._remote_settings_db:
            for filename in self.dm.listFiles(self.remote_idb_dir):
                if filename.endswith('ssegtnti.sqlite'):
                    self._remote_settings_db = posixpath.join(self.remote_idb_dir, filename)
                    break
            else:
                raise DMError("Could not find settings db in '%s'!" % self.remote_idb_dir)
        return self._remote_settings_db

    def which(self, binary):
        paths = os.environ.get('PATH', {}).split(os.pathsep)
        if self.bindir is not None and os.path.abspath(self.bindir) not in paths:
            paths.insert(0, os.path.abspath(self.bindir))
            os.environ['PATH'] = os.pathsep.join(paths)

        return find_executable(binary)

    def start_application(self):
        self.dm.shellCheckOutput(['start', 'b2g'])

    def stop_application(self):
        self.dm.shellCheckOutput(['stop', 'b2g'])

    def setup_profile(self, profile):
        # For some reason user.js in the profile doesn't get picked up.
        # Manually copy it over to prefs.js. See bug 1009730 for more details.
        self.dm.moveTree(posixpath.join(self.remote_profile, 'user.js'),
                         posixpath.join(self.remote_profile, 'prefs.js'))

        if self.dm.fileExists(posixpath.join(self.remote_profile, 'settings.json')):
            # On devices, settings.json is only read from the profile if
            # the system location doesn't exist.
            if self.dm.fileExists(self.remote_settings_json):
                self.dm.removeFile(self.remote_settings_json)

            # Delete existing settings db and create a new empty one to force new
            # settings to be loaded.
            self.dm.removeFile(self.remote_settings_db)
            self.dm.shellCheckOutput(['touch', self.remote_settings_db])

        # On devices, the webapps are located in /data/local/webapps instead of the profile.
        # In some cases we may need to replace the existing webapps, in others we may just
        # need to leave them in the profile. If the system app is present in the profile
        # webapps, it's a good indication that they should replace the existing ones wholesale.
        profile_webapps = posixpath.join(self.remote_profile, 'webapps')
        if self.dm.dirExists(posixpath.join(profile_webapps, 'system.gaiamobile.org')):
            self.dm.removeDir(self.remote_webapps_dir)
            self.dm.moveTree(profile_webapps, self.remote_webapps_dir)

        # On devices extensions are installed in the system dir
        extension_dir = os.path.join(profile.profile, 'extensions', 'staged')
        if os.path.isdir(extension_dir):
            # Copy the extensions to the B2G bundles dir.
            for filename in os.listdir(extension_dir):
                path = posixpath.join(self.remote_bundles_dir, filename)
                if self.dm.fileExists(path):
                    self.dm.removeFile(path)
            self.dm.pushDir(extension_dir, self.remote_bundles_dir)

    def cleanup_profile(self):
        # Delete any bundled extensions
        extension_dir = posixpath.join(self.remote_profile, 'extensions', 'staged')
        if self.dm.dirExists(extension_dir):
            for filename in self.dm.listFiles(extension_dir):
                try:
                    self.dm.removeDir(posixpath.join(self.remote_bundles_dir, filename))
                except DMError:
                    pass

        if self.dm.fileExists(posixpath.join(self.remote_profile, 'settings.json')):
            # Force settings.db to be restored to defaults
            self.dm.removeFile(self.remote_settings_db)
            self.dm.shellCheckOutput(['touch', self.remote_settings_db])


class FirefoxContext(object):
    profile_class = FirefoxProfile


class ThunderbirdContext(object):
    profile_class = ThunderbirdProfile


class MetroContext(object):
    profile_class = MetroFirefoxProfile

    def __init__(self, binary=None):
        self.binary = binary or os.environ.get('BROWSER_PATH', None)

    def wrap_command(self, command):
        immersive_helper_path = os.path.join(os.path.dirname(here),
                                             'resources',
                                             'metrotestharness.exe')
        command[:0] = [immersive_helper_path, '-firefoxpath']
        return command
