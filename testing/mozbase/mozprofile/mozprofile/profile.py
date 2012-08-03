# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

__all__ = ['Profile', 'FirefoxProfile', 'ThunderbirdProfile']

import os
import time
import tempfile
import uuid
from addons import AddonManager
from permissions import Permissions
from shutil import rmtree

try:
    import simplejson
except ImportError:
    import json as simplejson

class Profile(object):
    """Handles all operations regarding profile. Created new profiles, installs extensions,
    sets preferences and handles cleanup."""

    def __init__(self,
                 profile=None, # Path to the profile
                 addons=None,  # String of one or list of addons to install
                 addon_manifests=None,  # Manifest for addons, see http://ahal.ca/blog/2011/bulk-installing-fx-addons/
                 preferences=None, # Dictionary or class of preferences
                 locations=None, # locations to proxy
                 proxy=None, # setup a proxy - dict of server-loc,server-port,ssl-port
                 restore=True # If true remove all installed addons preferences when cleaning up
                 ):

        # if true, remove installed addons/prefs afterwards
        self.restore = restore

        # prefs files written to
        self.written_prefs = set()

        # our magic markers
        nonce = '%s %s' % (str(time.time()), uuid.uuid4())
        self.delimeters = ('#MozRunner Prefs Start %s' % nonce,'#MozRunner Prefs End %s' % nonce)

        # Handle profile creation
        self.create_new = not profile
        if profile:
            # Ensure we have a full path to the profile
            self.profile = os.path.abspath(os.path.expanduser(profile))
            if not os.path.exists(self.profile):
                os.makedirs(self.profile)
        else:
            self.profile = self.create_new_profile()

        # set preferences
        if hasattr(self.__class__, 'preferences'):
            # class preferences
            self.set_preferences(self.__class__.preferences)
        self._preferences = preferences
        if preferences:
            # supplied preferences
            if isinstance(preferences, dict):
                # unordered
                preferences = preferences.items()
            # sanity check
            assert not [i for i in preferences
                        if len(i) != 2]
        else:
            preferences = []
        self.set_preferences(preferences)

        # set permissions
        self._locations = locations # store this for reconstruction
        self._proxy = proxy
        self.permissions = Permissions(self.profile, locations)
        prefs_js, user_js = self.permissions.network_prefs(proxy)
        self.set_preferences(prefs_js, 'prefs.js')
        self.set_preferences(user_js)

        # handle addon installation
        self.addon_manager = AddonManager(self.profile)
        self.addon_manager.install_addons(addons, addon_manifests)

    def exists(self):
        """returns whether the profile exists or not"""
        return os.path.exists(self.profile)

    def reset(self):
        """
        reset the profile to the beginning state
        """
        self.cleanup()
        if self.create_new:
            profile = None
        else:
            profile = self.profile
        self.__init__(profile=profile,
                      addons=self.addon_manager.installed_addons,
                      addon_manifests=self.addon_manager.installed_manifests,
                      preferences=self._preferences,
                      locations=self._locations,
                      proxy = self._proxy)

    def create_new_profile(self):
        """Create a new clean profile in tmp which is a simple empty folder"""
        profile = tempfile.mkdtemp(suffix='.mozrunner')
        return profile


    ### methods for preferences

    def set_preferences(self, preferences, filename='user.js'):
        """Adds preferences dict to profile preferences"""


        # append to the file
        prefs_file = os.path.join(self.profile, filename)
        f = open(prefs_file, 'a')

        if preferences:

            # note what files we've touched
            self.written_prefs.add(filename)


            if isinstance(preferences, dict):
                # order doesn't matter
                preferences = preferences.items()

            # write the preferences
            f.write('\n%s\n' % self.delimeters[0])
            _prefs = [(simplejson.dumps(k), simplejson.dumps(v) )
                      for k, v in preferences]
            for _pref in _prefs:
                f.write('user_pref(%s, %s);\n' % _pref)
            f.write('%s\n' % self.delimeters[1])
        f.close()

    def pop_preferences(self, filename):
        """
        pop the last set of preferences added
        returns True if popped
        """

        lines = file(os.path.join(self.profile, filename)).read().splitlines()
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
        f = file(os.path.join(self.profile, 'user.js'), 'w')
        f.write(cleaned_prefs)
        f.close()
        return True

    def clean_preferences(self):
        """Removed preferences added by mozrunner."""
        for filename in self.written_prefs:
            if not os.path.exists(os.path.join(self.profile, filename)):
                # file has been deleted
                break
            while True:
                if not self.pop_preferences(filename):
                    break

    ### cleanup

    def _cleanup_error(self, function, path, excinfo):
        """ Specifically for windows we need to handle the case where the windows
            process has not yet relinquished handles on files, so we do a wait/try
            construct and timeout if we can't get a clear road to deletion
        """

        try:
            from exceptions import WindowsError
            from time import sleep
            def is_file_locked():
                return excinfo[0] is WindowsError and excinfo[1].winerror == 32

            if excinfo[0] is WindowsError and excinfo[1].winerror == 32:
                # Then we're on windows, wait to see if the file gets unlocked
                # we wait 10s
                count = 0
                while count < 10:
                    sleep(1)
                    try:
                        function(path)
                        break
                    except:
                        count += 1
        except ImportError:
            # We can't re-raise an error, so we'll hope the stuff above us will throw
            pass

    def cleanup(self):
        """Cleanup operations for the profile."""
        if self.restore:
            if self.create_new:
                if os.path.exists(self.profile):
                    rmtree(self.profile, onerror=self._cleanup_error)
            else:
                self.clean_preferences()
                self.addon_manager.clean_addons()
                self.permissions.clean_db()

    __del__ = cleanup

class FirefoxProfile(Profile):
    """Specialized Profile subclass for Firefox"""
    preferences = {# Don't automatically update the application
                   'app.update.enabled' : False,
                   # Don't restore the last open set of tabs if the browser has crashed
                   'browser.sessionstore.resume_from_crash': False,
                   # Don't check for the default web browser
                   'browser.shell.checkDefaultBrowser' : False,
                   # Don't warn on exit when multiple tabs are open
                   'browser.tabs.warnOnClose' : False,
                   # Don't warn when exiting the browser
                   'browser.warnOnQuit': False,
                   # Only install add-ons from the profile and the application scope
                   # Also ensure that those are not getting disabled.
                   # see: https://developer.mozilla.org/en/Installing_extensions
                   'extensions.enabledScopes' : 5,
                   'extensions.autoDisableScopes' : 10,
                   # Don't install distribution add-ons from the app folder
                   'extensions.installDistroAddons' : False,
                   # Dont' run the add-on compatibility check during start-up
                   'extensions.showMismatchUI' : False,
                   # Don't automatically update add-ons
                   'extensions.update.enabled'    : False,
                   # Don't open a dialog to show available add-on updates
                   'extensions.update.notifyUser' : False,
                   # Suppress automatic safe mode after crashes
                   'toolkit.startup.max_resumed_crashes' : -1,
                   # Enable test mode to run multiple tests in parallel
                   'focusmanager.testmode' : True,
                   }

class ThunderbirdProfile(Profile):
    preferences = {'extensions.update.enabled'    : False,
                   'extensions.update.notifyUser' : False,
                   'browser.shell.checkDefaultBrowser' : False,
                   'browser.tabs.warnOnClose' : False,
                   'browser.warnOnQuit': False,
                   'browser.sessionstore.resume_from_crash': False,
                   # prevents the 'new e-mail address' wizard on new profile
                   'mail.provider.enabled': False,
                   }
