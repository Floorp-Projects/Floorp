# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from abc import ABCMeta, abstractmethod
from distutils.spawn import find_executable
import os
import posixpath

from mozdevice import ADBAndroid
from mozprofile import (
    Profile,
    ChromeProfile,
    FirefoxProfile,
    ThunderbirdProfile
)

here = os.path.abspath(os.path.dirname(__file__))


def get_app_context(appname):
    context_map = {
        'chrome': ChromeContext,
        'default': DefaultContext,
        'fennec': FennecContext,
        'firefox': FirefoxContext,
        'thunderbird': ThunderbirdContext,
    }
    if appname not in context_map:
        raise KeyError("Application '%s' not supported!" % appname)
    return context_map[appname]


class DefaultContext(object):
    profile_class = Profile


class RemoteContext(object):
    __metaclass__ = ABCMeta
    device = None
    _remote_profile = None
    _adb = None
    profile_class = Profile
    _bindir = None
    remote_test_root = ''
    remote_process = None

    @property
    def bindir(self):
        if self._bindir is None:
            paths = [find_executable('emulator')]
            paths = [p for p in paths if p is not None if os.path.isfile(p)]
            if not paths:
                self._bindir = ''
            else:
                self._bindir = os.path.dirname(paths[0])
        return self._bindir

    @property
    def adb(self):
        if not self._adb:
            paths = [os.environ.get('ADB'),
                     os.environ.get('ADB_PATH'),
                     self.which('adb')]
            paths = [p for p in paths if p is not None if os.path.isfile(p)]
            if not paths:
                raise OSError(
                    'Could not find the adb binary, make sure it is on your'
                    'path or set the $ADB_PATH environment variable.')
            self._adb = paths[0]
        return self._adb

    @property
    def remote_profile(self):
        if not self._remote_profile:
            self._remote_profile = posixpath.join(self.remote_test_root,
                                                  'profile')
        return self._remote_profile

    def which(self, binary):
        paths = os.environ.get('PATH', {}).split(os.pathsep)
        if self.bindir is not None and os.path.abspath(self.bindir) not in paths:
            paths.insert(0, os.path.abspath(self.bindir))
            os.environ['PATH'] = os.pathsep.join(paths)

        return find_executable(binary)

    @abstractmethod
    def stop_application(self):
        """ Run (device manager) command to stop application. """
        pass


class FennecContext(RemoteContext):
    _remote_profiles_ini = None
    _remote_test_root = None

    def __init__(self, app=None, adb_path=None, avd_home=None, device_serial=None):
        self._adb = adb_path
        self.avd_home = avd_home
        self.remote_process = app
        self.device_serial = device_serial
        self.device = ADBAndroid(adb=self.adb, device=device_serial)

    def stop_application(self):
        self.device.stop_application(self.remote_process)

    @property
    def remote_test_root(self):
        if not self._remote_test_root:
            self._remote_test_root = self.device.test_root
        return self._remote_test_root

    @property
    def remote_profiles_ini(self):
        if not self._remote_profiles_ini:
            self._remote_profiles_ini = posixpath.join(
                '/data', 'data', self.remote_process,
                'files', 'mozilla', 'profiles.ini'
            )
        return self._remote_profiles_ini


class FirefoxContext(object):
    profile_class = FirefoxProfile


class ThunderbirdContext(object):
    profile_class = ThunderbirdProfile


class ChromeContext(object):
    profile_class = ChromeProfile
