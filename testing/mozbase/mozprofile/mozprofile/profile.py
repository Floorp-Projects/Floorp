# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozprofile.
#
# The Initial Developer of the Original Code is
#   The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2008-2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mikeal Rogers <mikeal.rogers@gmail.com>
#   Clint Talbert <ctalbert@mozilla.com>
#   Henrik Skupin <hskupin@mozilla.com>
#   Jeff Hammel <jhammel@mozilla.com>
#   Andrew Halberstadt <halbersa@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

__all__ = ['Profile', 'FirefoxProfile', 'ThunderbirdProfile']

import os
import tempfile
from addons import AddonManager
from permissions import PermissionsManager
from shutil import rmtree

try:
    import simplejson
except ImportError:
    import json as simplejson

class Profile(object):
    """Handles all operations regarding profile. Created new profiles, installs extensions,
    sets preferences and handles cleanup."""

    def __init__(self, profile=None, addons=None, addon_manifests=None, preferences=None, locations=None, proxy=False, restore=True):

        # if true, remove installed addons/prefs afterwards
        self.restore = restore

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
        self.permission_manager = PermissionsManager(self.profile, locations)
        prefs_js, user_js = self.permission_manager.getNetworkPreferences(proxy)
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
                      addons=self.addon_manager.addons,
                      addon_manifests=self.addon_manager.manifests,
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

        if isinstance(preferences, dict):
            # order doesn't matter
            preferences = preferences.items()

        # write the preferences
        if preferences:
            f.write('\n#MozRunner Prefs Start\n')
            _prefs = [(simplejson.dumps(k), simplejson.dumps(v) )
                      for k, v in preferences]
            for _pref in _prefs:
                f.write('user_pref(%s, %s);\n' % _pref)
            f.write('#MozRunner Prefs End\n')
        f.close()

    def pop_preferences(self):
        """
        pop the last set of preferences added
        returns True if popped
        """

        # our magic markers
        delimeters = ('#MozRunner Prefs Start', '#MozRunner Prefs End')

        lines = file(os.path.join(self.profile, 'user.js')).read().splitlines()
        def last_index(_list, value):
            """
            returns the last index of an item;
            this should actually be part of python code but it isn't
            """
            for index in reversed(range(len(_list))):
                if _list[index] == value:
                    return index
        s = last_index(lines, delimeters[0])
        e = last_index(lines, delimeters[1])

        # ensure both markers are found
        if s is None:
            assert e is None, '%s found without %s' % (delimeters[1], delimeters[0])
            return False # no preferences found
        elif e is None:
            assert e is None, '%s found without %s' % (delimeters[0], delimeters[1])

        # ensure the markers are in the proper order
        assert e > s, '%s found at %s, while %s found at %s' (delimeter[1], e, delimeter[0], s)

        # write the prefs
        cleaned_prefs = '\n'.join(lines[:s] + lines[e+1:])
        f = file(os.path.join(self.profile, 'user.js'), 'w')
        return True

    def clean_preferences(self):
        """Removed preferences added by mozrunner."""
        while True:
            if not self.pop_preferences():
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
        """Cleanup operations on the profile."""
        if self.restore:
            if self.create_new:
                if os.path.exists(self.profile):
                    rmtree(self.profile, onerror=self._cleanup_error)
            else:
                self.clean_preferences()
                self.addon_manager.clean_addons()
                self.permission_manager.clean_permissions()

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
