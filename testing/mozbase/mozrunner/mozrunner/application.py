# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from distutils.spawn import find_executable
import glob
import os
import posixpath
import sys

from mozdevice import DeviceManagerADB
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
    profile_class = Profile

    def __init__(self, b2g_home=None, adb_path=None):
        self.homedir = b2g_home or os.environ.get('B2G_HOME')
        if not self.homedir:
            raise EnvironmentError('Must define B2G_HOME or pass the b2g_home parameter')

        if not os.path.isdir(self.homedir):
            raise OSError('Homedir \'%s\' does not exist!' % self.homedir)

        self._adb = adb_path
        self.update_tools = os.path.join(self.homedir, 'tools', 'update-tools')
        self.fastboot = self.which('fastboot')

        self.remote_binary = '/system/bin/b2g.sh'
        self.remote_process = '/system/b2g/b2g'
        self.remote_bundles_dir = '/system/b2g/distribution/bundles'
        self.remote_busybox = '/system/bin/busybox'
        self.remote_profiles_ini = '/data/b2g/mozilla/profiles.ini'
        self.remote_test_root = '/data/local/tests'

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
        if not self._bindir:
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


    def which(self, binary):
        if self.bindir not in sys.path:
            sys.path.insert(0, self.bindir)

        return find_executable(binary, os.pathsep.join(sys.path))

    def stop_application(self):
        self.dm.shellCheckOutput(['stop', 'b2g'])

        # For some reason user.js in the profile doesn't get picked up.
        # Manually copy it over to prefs.js. See bug 1009730 for more details.
        self.dm.moveTree(posixpath.join(self.remote_profile, 'user.js'),
                         posixpath.join(self.remote_profile, 'prefs.js'))


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
