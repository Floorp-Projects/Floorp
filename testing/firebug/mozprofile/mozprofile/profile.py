# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

__all__ = ['Profile', 'FirefoxProfile', 'ThunderbirdProfile', 'print_addon_ids']

import os
import sys
import tempfile
import zipfile
from xml.dom import minidom

try:
    import simplejson
except ImportError:
    import json as simplejson

# Use dir_util for copy/rm operations because shutil is all kinds of broken
from distutils import dir_util
copytree = dir_util.copy_tree
rmtree = dir_util.remove_tree


class Profile(object):
    """Handles all operations regarding profile. Created new profiles, installs extensions,
    sets preferences and handles cleanup."""

    def __init__(self, profile=None, addons=None, preferences=None):
                 
        # Handle profile creation
        self.create_new = not profile
        if profile:
            self.profile = profile
        else:
            self.profile = self.create_new_profile()

        # set preferences from class preferences
        if hasattr(self.__class__, 'preferences'):
            self.preferences = self.__class__.preferences.copy()
        else:
            self.preferences = {}
        self.preferences.update(preferences or {})
        self.set_preferences(self.preferences)
 
        # handle addon installation
        self.addons_installed = []
        self.addons = addons or []
        for addon in self.addons:
            self.install_addon(addon)

    def reset(self):
        """
        reset the profile to the beginning state
        """
        self.cleanup()
        if self.create_new:
            self.__init__(addons=self.addons, preferences=self.preferences)
        else:
            self.__init__(profile=self.profile, addons=self.addons, preferences=self.preferences)

    def create_new_profile(self):
        """Create a new clean profile in tmp which is a simple empty folder"""
        profile = tempfile.mkdtemp(suffix='.mozrunner')
        return profile

    ### methods related to addons

    @classmethod
    def addon_id(self, addon_path):
        """
        return the id for a given addon, or None if not found
        - addon_path : path to the addon directory
        """
        
        def find_id(desc):
            """finds the addon id give its description"""
            
            addon_id = None
            for elem in desc:
                apps = elem.getElementsByTagName('em:targetApplication')
                if apps:
                    for app in apps:
                        # remove targetApplication nodes, they contain id's we aren't interested in
                        elem.removeChild(app)
                    if elem.getElementsByTagName('em:id'):
                        addon_id = str(elem.getElementsByTagName('em:id')[0].firstChild.data)
                    elif elem.hasAttribute('em:id'):
                        addon_id = str(elem.getAttribute('em:id'))
            return addon_id

        doc = minidom.parse(os.path.join(addon_path, 'install.rdf')) 

        for tag in 'Description', 'RDF:Description':
            desc = doc.getElementsByTagName(tag)
            addon_id = find_id(desc)
            if addon_id:
                return addon_id

    def install_addon(self, path):
        """Installs the given addon or directory of addons in the profile."""

        # if the addon is a directory, install all addons in it
        addons = [path]
        if not path.endswith('.xpi') and not os.path.exists(os.path.join(path, 'install.rdf')):
            addons = [os.path.join(path, x) for x in os.listdir(path)]
           
        for addon in addons:
            if addon.endswith('.xpi'):
                tmpdir = tempfile.mkdtemp(suffix = "." + os.path.split(addon)[-1])
                compressed_file = zipfile.ZipFile(addon, "r")
                for name in compressed_file.namelist():
                    if name.endswith('/'):
                        os.makedirs(os.path.join(tmpdir, name))
                    else:
                        if not os.path.isdir(os.path.dirname(os.path.join(tmpdir, name))):
                            os.makedirs(os.path.dirname(os.path.join(tmpdir, name)))
                        data = compressed_file.read(name)
                        f = open(os.path.join(tmpdir, name), 'wb')
                        f.write(data)
                        f.close()
                addon = tmpdir

            # determine the addon id
            addon_id = Profile.addon_id(addon)
            assert addon_id is not None, "The addon id could not be found: %s" % addon
 
            # copy the addon to the profile
            addon_path = os.path.join(self.profile, 'extensions', addon_id)
            copytree(addon, addon_path, preserve_symlinks=1)
            self.addons_installed.append(addon_path)

    def clean_addons(self):
        """Cleans up addons in the profile."""
        for addon in self.addons_installed:
            if os.path.isdir(addon):
                rmtree(addon)

    ### methods for preferences

    def set_preferences(self, preferences):
        """Adds preferences dict to profile preferences"""
        
        prefs_file = os.path.join(self.profile, 'user.js')
        
        # Ensure that the file exists first otherwise create an empty file
        if os.path.isfile(prefs_file):
            f = open(prefs_file, 'a+')
        else:
            f = open(prefs_file, 'w')

        f.write('\n#MozRunner Prefs Start\n')

        pref_lines = ['user_pref(%s, %s);' %
                      (simplejson.dumps(k), simplejson.dumps(v) ) for k, v in
                       preferences.items()]
        for line in pref_lines:
            f.write(line+'\n')
        f.write('#MozRunner Prefs End\n')
        f.flush() ; f.close()

    def clean_preferences(self):
        """Removed preferences added by mozrunner."""
        lines = open(os.path.join(self.profile, 'user.js'), 'r').read().splitlines()
        s = lines.index('#MozRunner Prefs Start') ; e = lines.index('#MozRunner Prefs End')
        cleaned_prefs = '\n'.join(lines[:s] + lines[e+1:])
        f = open(os.path.join(self.profile, 'user.js'), 'w')
        f.write(cleaned_prefs) ; f.flush() ; f.close()

    ### cleanup
 
    def cleanup(self):
        """Cleanup operations on the profile."""
        if self.create_new:
            if os.path.exists(self.profile):
                rmtree(self.profile)
        else:
            self.clean_preferences()
            self.clean_addons()

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
                   # Only install add-ons from the profile and the app folder
                   'extensions.enabledScopes' : 5,
                   # Dont' run the add-on compatibility check during start-up
                   'extensions.showMismatchUI' : False,
                   # Don't automatically update add-ons
                   'extensions.update.enabled'    : False,
                   # Don't open a dialog to show available add-on updates
                   'extensions.update.notifyUser' : False,
                   # Suppress automatic safe mode after crashes
                   'toolkit.startup.max_resumed_crashes' : -1,
                   }

class ThunderbirdProfile(Profile):
    preferences = {'extensions.update.enabled'    : False,
                   'extensions.update.notifyUser' : False,
                   'browser.shell.checkDefaultBrowser' : False,
                   'browser.tabs.warnOnClose' : False,
                   'browser.warnOnQuit': False,
                   'browser.sessionstore.resume_from_crash': False,
                   }


def print_addon_ids(args=sys.argv[1:]):
    """print addon ids for testing"""
    for arg in args:
        print Profile.addon_id(arg)
