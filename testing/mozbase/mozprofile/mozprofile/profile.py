# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

__all__ = ['Profile',
           'FirefoxProfile',
           'MetroFirefoxProfile',
           'ThunderbirdProfile']

import os
import time
import tempfile
import types
import uuid

from addons import AddonManager
import mozfile
from permissions import Permissions
from prefs import Preferences
from shutil import copytree
from webapps import WebappCollection


class Profile(object):
    """Handles all operations regarding profile.

    Creating new profiles, installing add-ons, setting preferences and
    handling cleanup.
    """

    def __init__(self, profile=None, addons=None, addon_manifests=None, apps=None,
                 preferences=None, locations=None, proxy=None, restore=True):
        """
        :param profile: Path to the profile
        :param addons: String of one or list of addons to install
        :param addon_manifests: Manifest for addons (see http://bit.ly/17jQ7i6)
        :param apps: Dictionary or class of webapps to install
        :param preferences: Dictionary or class of preferences
        :param locations: ServerLocations object
        :param proxy: Setup a proxy
        :param restore: Flag for removing all custom settings during cleanup
        """
        self._addons = addons
        self._addon_manifests = addon_manifests
        self._apps = apps
        self._locations = locations
        self._proxy = proxy

        # Prepare additional preferences
        if preferences:
            if isinstance(preferences, dict):
                # unordered
                preferences = preferences.items()

            # sanity check
            assert not [i for i in preferences if len(i) != 2]
        else:
            preferences = []
        self._preferences = preferences

        # Handle profile creation
        self.create_new = not profile
        if profile:
            # Ensure we have a full path to the profile
            self.profile = os.path.abspath(os.path.expanduser(profile))
        else:
            self.profile = tempfile.mkdtemp(suffix='.mozrunner')

        self.restore = restore

        # Initialize all class members
        self._internal_init()

    def _internal_init(self):
        """Internal: Initialize all class members to their default value"""

        if not os.path.exists(self.profile):
            os.makedirs(self.profile)

        # Preferences files written to
        self.written_prefs = set()

        # Our magic markers
        nonce = '%s %s' % (str(time.time()), uuid.uuid4())
        self.delimeters = ('#MozRunner Prefs Start %s' % nonce,
                           '#MozRunner Prefs End %s' % nonce)

        # If sub-classes want to set default preferences
        if hasattr(self.__class__, 'preferences'):
            self.set_preferences(self.__class__.preferences)
        # Set additional preferences
        self.set_preferences(self._preferences)

        self.permissions = Permissions(self.profile, self._locations)
        prefs_js, user_js = self.permissions.network_prefs(self._proxy)
        self.set_preferences(prefs_js, 'prefs.js')
        self.set_preferences(user_js)

        # handle add-on installation
        self.addon_manager = AddonManager(self.profile, restore=self.restore)
        self.addon_manager.install_addons(self._addons, self._addon_manifests)

        # handle webapps
        self.webapps = WebappCollection(profile=self.profile, apps=self._apps)
        self.webapps.update_manifests()

    def __del__(self):
        self.cleanup()

    ### cleanup

    def cleanup(self):
        """Cleanup operations for the profile."""

        if self.restore:
            # If copies of those class instances exist ensure we correctly
            # reset them all (see bug 934484)
            self.clean_preferences()
            if getattr(self, 'addon_manager', None) is not None:
                self.addon_manager.clean()
            if getattr(self, 'permissions', None) is not None:
                self.permissions.clean_db()
            if getattr(self, 'webapps', None) is not None:
                self.webapps.clean()

            # If it's a temporary profile we have to remove it
            if self.create_new:
                mozfile.remove(self.profile)

    def reset(self):
        """
        reset the profile to the beginning state
        """
        self.cleanup()

        self._internal_init()

    def clean_preferences(self):
        """Removed preferences added by mozrunner."""
        for filename in self.written_prefs:
            if not os.path.exists(os.path.join(self.profile, filename)):
                # file has been deleted
                break
            while True:
                if not self.pop_preferences(filename):
                    break

    @classmethod
    def clone(cls, path_from, path_to=None, **kwargs):
        """Instantiate a temporary profile via cloning
        - path: path of the basis to clone
        - kwargs: arguments to the profile constructor
        """
        if not path_to:
            tempdir = tempfile.mkdtemp() # need an unused temp dir name
            mozfile.remove(tempdir) # copytree requires that dest does not exist
            path_to = tempdir
        copytree(path_from, path_to)

        def cleanup_clone(fn):
            """Deletes a cloned profile when restore is True"""
            def wrapped(self):
                fn(self)
                if self.restore and os.path.exists(self.profile):
                    mozfile.remove(self.profile)
            return wrapped

        c = cls(path_to, **kwargs)
        c.__del__ = c.cleanup = types.MethodType(cleanup_clone(cls.cleanup), c)
        return c

    def exists(self):
        """returns whether the profile exists or not"""
        return os.path.exists(self.profile)

    ### methods for preferences

    def set_preferences(self, preferences, filename='user.js'):
        """Adds preferences dict to profile preferences"""

        # append to the file
        prefs_file = os.path.join(self.profile, filename)
        f = open(prefs_file, 'a')

        if preferences:

            # note what files we've touched
            self.written_prefs.add(filename)

            # opening delimeter
            f.write('\n%s\n' % self.delimeters[0])

            # write the preferences
            Preferences.write(f, preferences)

            # closing delimeter
            f.write('%s\n' % self.delimeters[1])

        f.close()

    def set_persistent_preferences(self, preferences):
        """
        Adds preferences dict to profile preferences and save them during a
        profile reset
        """

        # this is a dict sometimes, convert
        if isinstance(preferences, dict):
            preferences = preferences.items()

        # add new prefs to preserve them during reset
        for new_pref in preferences:
            # if dupe remove item from original list
            self._preferences = [
                pref for pref in self._preferences if not new_pref[0] == pref[0]]
            self._preferences.append(new_pref)

        self.set_preferences(preferences, filename='user.js')

    def pop_preferences(self, filename):
        """
        pop the last set of preferences added
        returns True if popped
        """

        path = os.path.join(self.profile, filename)
        with file(path) as f:
            lines = f.read().splitlines()
        def last_index(_list, value):
            """
            returns the last index of an item;
            this should actually be part of python code but it isn't
            """
            for index in reversed(range(len(_list))):
                if _list[index] == value:
                    return index
        s = last_index(lines, self.delimeters[0])
        e = last_index(lines, self.delimeters[1])

        # ensure both markers are found
        if s is None:
            assert e is None, '%s found without %s' % (self.delimeters[1], self.delimeters[0])
            return False # no preferences found
        elif e is None:
            assert s is None, '%s found without %s' % (self.delimeters[0], self.delimeters[1])

        # ensure the markers are in the proper order
        assert e > s, '%s found at %s, while %s found at %s' % (self.delimeters[1], e, self.delimeters[0], s)

        # write the prefs
        cleaned_prefs = '\n'.join(lines[:s] + lines[e+1:])
        with file(path, 'w') as f:
            f.write(cleaned_prefs)
        return True

    ### methods for introspection

    def summary(self, return_parts=False):
        """
        returns string summarizing profile information.
        if return_parts is true, return the (Part_name, value) list
        of tuples instead of the assembled string
        """

        parts = [('Path', self.profile)] # profile path

        # directory tree
        parts.append(('Files', '\n%s' % mozfile.tree(self.profile)))

        # preferences
        for prefs_file in ('user.js', 'prefs.js'):
            path = os.path.join(self.profile, prefs_file)
            if os.path.exists(path):

                # prefs that get their own section
                # This is currently only 'network.proxy.autoconfig_url'
                # but could be expanded to include others
                section_prefs = ['network.proxy.autoconfig_url']
                line_length = 80
                line_length_buffer = 10 # buffer for 80 character display: length = 80 - len(key) - len(': ') - line_length_buffer
                line_length_buffer += len(': ')
                def format_value(key, value):
                    if key not in section_prefs:
                        return value
                    max_length = line_length - len(key) - line_length_buffer
                    if len(value) > max_length:
                        value = '%s...' % value[:max_length]
                    return value

                prefs = Preferences.read_prefs(path)
                if prefs:
                    prefs = dict(prefs)
                    parts.append((prefs_file,
                    '\n%s' %('\n'.join(['%s: %s' % (key, format_value(key, prefs[key]))
                                        for key in sorted(prefs.keys())
                                        ]))))

                    # Currently hardcorded to 'network.proxy.autoconfig_url'
                    # but could be generalized, possibly with a generalized (simple)
                    # JS-parser
                    network_proxy_autoconfig = prefs.get('network.proxy.autoconfig_url')
                    if network_proxy_autoconfig and network_proxy_autoconfig.strip():
                        network_proxy_autoconfig = network_proxy_autoconfig.strip()
                        lines = network_proxy_autoconfig.replace(';', ';\n').splitlines()
                        lines = [line.strip() for line in lines]
                        origins_string = 'var origins = ['
                        origins_end = '];'
                        if origins_string in lines[0]:
                            start = lines[0].find(origins_string)
                            end = lines[0].find(origins_end, start);
                            splitline = [lines[0][:start],
                                         lines[0][start:start+len(origins_string)-1],
                                         ]
                            splitline.extend(lines[0][start+len(origins_string):end].replace(',', ',\n').splitlines())
                            splitline.append(lines[0][end:])
                            lines[0:1] = [i.strip() for i in splitline]
                        parts.append(('Network Proxy Autoconfig, %s' % (prefs_file),
                                      '\n%s' % '\n'.join(lines)))

        if return_parts:
            return parts

        retval = '%s\n' % ('\n\n'.join(['[%s]: %s' % (key, value)
                                        for key, value in parts]))
        return retval

    __str__ = summary


class FirefoxProfile(Profile):
    """Specialized Profile subclass for Firefox"""

    preferences = {# Don't automatically update the application
                   'app.update.enabled' : False,
                   # Don't restore the last open set of tabs if the browser has crashed
                   'browser.sessionstore.resume_from_crash': False,
                   # Don't check for the default web browser during startup
                   'browser.shell.checkDefaultBrowser' : False,
                   # Don't warn on exit when multiple tabs are open
                   'browser.tabs.warnOnClose' : False,
                   # Don't warn when exiting the browser
                   'browser.warnOnQuit': False,
                   # Don't send Firefox health reports to the production server
                   'datareporting.healthreport.documentServerURI' : 'http://%(server)s/healthreport/',
                   # Only install add-ons from the profile and the application scope
                   # Also ensure that those are not getting disabled.
                   # see: https://developer.mozilla.org/en/Installing_extensions
                   'extensions.enabledScopes' : 5,
                   'extensions.autoDisableScopes' : 10,
                   # Don't send the list of installed addons to AMO
                   'extensions.getAddons.cache.enabled' : False,
                   # Don't install distribution add-ons from the app folder
                   'extensions.installDistroAddons' : False,
                   # Dont' run the add-on compatibility check during start-up
                   'extensions.showMismatchUI' : False,
                   # Don't automatically update add-ons
                   'extensions.update.enabled'    : False,
                   # Don't open a dialog to show available add-on updates
                   'extensions.update.notifyUser' : False,
                   # Enable test mode to run multiple tests in parallel
                   'focusmanager.testmode' : True,
                   # Enable test mode to not raise an OS level dialog for location sharing
                   'geo.provider.testing' : True,
                   # Suppress delay for main action in popup notifications
                   'security.notification_enable_delay' : 0,
                   # Suppress automatic safe mode after crashes
                   'toolkit.startup.max_resumed_crashes' : -1,
                   # Don't report telemetry information
                   'toolkit.telemetry.enabled' : False,
                   }

class MetroFirefoxProfile(Profile):
    """Specialized Profile subclass for Firefox Metro"""

    preferences = {# Don't automatically update the application for desktop and metro build
                   'app.update.enabled' : False,
                   'app.update.metro.enabled' : False,
                   # Dismiss first run content overlay
                   'browser.firstrun-content.dismissed' : True,
                   # Don't restore the last open set of tabs if the browser has crashed
                   'browser.sessionstore.resume_from_crash': False,
                   # Don't check for the default web browser during startup
                   'browser.shell.checkDefaultBrowser' : False,
                   # Don't send Firefox health reports to the production server
                   'datareporting.healthreport.documentServerURI' : 'http://%(server)s/healthreport/',
                   # Enable extensions
                   'extensions.defaultProviders.enabled' : True,
                   # Only install add-ons from the profile and the application scope
                   # Also ensure that those are not getting disabled.
                   # see: https://developer.mozilla.org/en/Installing_extensions
                   'extensions.enabledScopes' : 5,
                   'extensions.autoDisableScopes' : 10,
                   # Don't send the list of installed addons to AMO
                   'extensions.getAddons.cache.enabled' : False,
                   # Don't install distribution add-ons from the app folder
                   'extensions.installDistroAddons' : False,
                   # Dont' run the add-on compatibility check during start-up
                   'extensions.showMismatchUI' : False,
                   # Disable strict compatibility checks to allow add-ons enabled by default
                   'extensions.strictCompatibility' : False,
                   # Don't automatically update add-ons
                   'extensions.update.enabled'    : False,
                   # Don't open a dialog to show available add-on updates
                   'extensions.update.notifyUser' : False,
                   # Enable test mode to run multiple tests in parallel
                   'focusmanager.testmode' : True,
                   # Suppress delay for main action in popup notifications
                   'security.notification_enable_delay' : 0,
                   # Suppress automatic safe mode after crashes
                   'toolkit.startup.max_resumed_crashes' : -1,
                   # Don't report telemetry information
                   'toolkit.telemetry.enabled' : False,
                   }

class ThunderbirdProfile(Profile):
    """Specialized Profile subclass for Thunderbird"""

    preferences = {'extensions.update.enabled'    : False,
                   'extensions.update.notifyUser' : False,
                   'browser.shell.checkDefaultBrowser' : False,
                   'browser.tabs.warnOnClose' : False,
                   'browser.warnOnQuit': False,
                   'browser.sessionstore.resume_from_crash': False,
                   # prevents the 'new e-mail address' wizard on new profile
                   'mail.provider.enabled': False,
                   }
