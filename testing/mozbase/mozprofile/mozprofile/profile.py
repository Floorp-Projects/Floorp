# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import six
import json
import os
import platform
import tempfile
import time
import uuid
from abc import ABCMeta, abstractmethod, abstractproperty
from shutil import copytree
from io import open

import mozfile
from six import string_types, python_2_unicode_compatible

if six.PY3:

    def unicode(input):
        return input


from .addons import AddonManager
from .permissions import Permissions
from .prefs import Preferences

__all__ = [
    "BaseProfile",
    "ChromeProfile",
    "ChromiumProfile",
    "Profile",
    "FirefoxProfile",
    "ThunderbirdProfile",
    "create_profile",
]


@six.add_metaclass(ABCMeta)
class BaseProfile(object):
    def __init__(self, profile=None, addons=None, preferences=None, restore=True):
        """Create a new Profile.

        All arguments are optional.

        :param profile: Path to a profile. If not specified, a new profile
                        directory will be created.
        :param addons: List of paths to addons which should be installed in the profile.
        :param preferences: Dict of preferences to set in the profile.
        :param restore: Whether or not to clean up any modifications made to this profile
                        (default True).
        """
        self._addons = addons or []

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
        self.restore = restore
        self.create_new = not profile
        if profile:
            # Ensure we have a full path to the profile
            self.profile = os.path.abspath(os.path.expanduser(profile))
        else:
            self.profile = tempfile.mkdtemp(suffix=".mozrunner")

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.cleanup()

    def __del__(self):
        self.cleanup()

    def cleanup(self):
        """Cleanup operations for the profile."""

        if self.restore:
            # If it's a temporary profile we have to remove it
            if self.create_new:
                mozfile.remove(self.profile)

    @abstractmethod
    def _reset(self):
        pass

    def reset(self):
        """
        reset the profile to the beginning state
        """
        self.cleanup()
        self._reset()

    @abstractmethod
    def set_preferences(self, preferences, filename="user.js"):
        pass

    @abstractproperty
    def preference_file_names(self):
        """A tuple of file basenames expected to contain preferences."""

    def merge(self, other, interpolation=None):
        """Merges another profile into this one.

        This will handle pref files matching the profile's
        `preference_file_names` property, and any addons in the
        other/extensions directory.
        """
        for basename in os.listdir(other):
            if basename not in self.preference_file_names:
                continue

            path = os.path.join(other, basename)
            try:
                prefs = Preferences.read_json(path)
            except ValueError:
                prefs = Preferences.read_prefs(path, interpolation=interpolation)
            self.set_preferences(prefs, filename=basename)

        extension_dir = os.path.join(other, "extensions")
        if not os.path.isdir(extension_dir):
            return

        for basename in os.listdir(extension_dir):
            path = os.path.join(extension_dir, basename)

            if self.addons.is_addon(path):
                self._addons.append(path)
                self.addons.install(path)

    @classmethod
    def clone(cls, path_from, path_to=None, ignore=None, **kwargs):
        """Instantiate a temporary profile via cloning
        - path: path of the basis to clone
        - ignore: callable passed to shutil.copytree
        - kwargs: arguments to the profile constructor
        """
        if not path_to:
            tempdir = tempfile.mkdtemp()  # need an unused temp dir name
            mozfile.remove(tempdir)  # copytree requires that dest does not exist
            path_to = tempdir
        copytree(path_from, path_to, ignore=ignore, ignore_dangling_symlinks=True)

        c = cls(path_to, **kwargs)
        c.create_new = True  # deletes a cloned profile when restore is True
        return c

    def exists(self):
        """returns whether the profile exists or not"""
        return os.path.exists(self.profile)


@python_2_unicode_compatible
class Profile(BaseProfile):
    """Handles all operations regarding profile.

    Creating new profiles, installing add-ons, setting preferences and
    handling cleanup.

    The files associated with the profile will be removed automatically after
    the object is garbage collected: ::

      profile = Profile()
      print profile.profile  # this is the path to the created profile
      del profile
      # the profile path has been removed from disk

    :meth:`cleanup` is called under the hood to remove the profile files. You
    can ensure this method is called (even in the case of exception) by using
    the profile as a context manager: ::

      with Profile() as profile:
          # do things with the profile
          pass
      # profile.cleanup() has been called here
    """

    preference_file_names = ("user.js", "prefs.js")

    def __init__(
        self,
        profile=None,
        addons=None,
        preferences=None,
        locations=None,
        proxy=None,
        restore=True,
        whitelistpaths=None,
        **kwargs
    ):
        """
        :param profile: Path to the profile
        :param addons: String of one or list of addons to install
        :param preferences: Dictionary or class of preferences
        :param locations: ServerLocations object
        :param proxy: Setup a proxy
        :param restore: Flag for removing all custom settings during cleanup
        :param whitelistpaths: List of paths to pass to Firefox to allow read
            access to from the content process sandbox.
        """
        super(Profile, self).__init__(
            profile=profile,
            addons=addons,
            preferences=preferences,
            restore=restore,
            **kwargs
        )

        self._locations = locations
        self._proxy = proxy
        self._whitelistpaths = whitelistpaths

        # Initialize all class members
        self._reset()

    def _reset(self):
        """Internal: Initialize all class members to their default value"""

        if not os.path.exists(self.profile):
            os.makedirs(self.profile)

        # Preferences files written to
        self.written_prefs = set()

        # Our magic markers
        nonce = "%s %s" % (str(time.time()), uuid.uuid4())
        self.delimeters = (
            "#MozRunner Prefs Start %s" % nonce,
            "#MozRunner Prefs End %s" % nonce,
        )

        # If sub-classes want to set default preferences
        if hasattr(self.__class__, "preferences"):
            self.set_preferences(self.__class__.preferences)
        # Set additional preferences
        self.set_preferences(self._preferences)

        self.permissions = Permissions(self.profile, self._locations)
        prefs_js, user_js = self.permissions.network_prefs(self._proxy)

        if self._whitelistpaths:
            # On macOS we don't want to support a generalized read whitelist,
            # and the macOS sandbox policy language doesn't have support for
            # lists, so we handle these specially.
            if platform.system() == "Darwin":
                assert len(self._whitelistpaths) <= 2
                if len(self._whitelistpaths) == 2:
                    prefs_js.append(
                        (
                            "security.sandbox.content.mac.testing_read_path2",
                            self._whitelistpaths[1],
                        )
                    )
                prefs_js.append(
                    (
                        "security.sandbox.content.mac.testing_read_path1",
                        self._whitelistpaths[0],
                    )
                )
            else:
                prefs_js.append(
                    (
                        "security.sandbox.content.read_path_whitelist",
                        ",".join(self._whitelistpaths),
                    )
                )
        self.set_preferences(prefs_js, "prefs.js")
        self.set_preferences(user_js)

        # handle add-on installation
        self.addons = AddonManager(self.profile, restore=self.restore)
        self.addons.install(self._addons)

    def cleanup(self):
        """Cleanup operations for the profile."""

        if self.restore:
            # If copies of those class instances exist ensure we correctly
            # reset them all (see bug 934484)
            self.clean_preferences()
            if getattr(self, "addons", None) is not None:
                self.addons.clean()
            if getattr(self, "permissions", None) is not None:
                self.permissions.clean_db()
        super(Profile, self).cleanup()

    def clean_preferences(self):
        """Removed preferences added by mozrunner."""
        for filename in self.written_prefs:
            if not os.path.exists(os.path.join(self.profile, filename)):
                # file has been deleted
                break
            while True:
                if not self.pop_preferences(filename):
                    break

    # methods for preferences

    def set_preferences(self, preferences, filename="user.js"):
        """Adds preferences dict to profile preferences"""
        prefs_file = os.path.join(self.profile, filename)
        with open(prefs_file, "a") as f:
            if not preferences:
                return

            # note what files we've touched
            self.written_prefs.add(filename)

            # opening delimeter
            f.write(unicode("\n%s\n" % self.delimeters[0]))

            Preferences.write(f, preferences)

            # closing delimeter
            f.write(unicode("%s\n" % self.delimeters[1]))

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
                pref for pref in self._preferences if not new_pref[0] == pref[0]
            ]
            self._preferences.append(new_pref)

        self.set_preferences(preferences, filename="user.js")

    def pop_preferences(self, filename):
        """
        pop the last set of preferences added
        returns True if popped
        """

        path = os.path.join(self.profile, filename)
        with open(path, "r", encoding="utf-8") as f:
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
            assert e is None, "%s found without %s" % (
                self.delimeters[1],
                self.delimeters[0],
            )
            return False  # no preferences found
        elif e is None:
            assert s is None, "%s found without %s" % (
                self.delimeters[0],
                self.delimeters[1],
            )

        # ensure the markers are in the proper order
        assert e > s, "%s found at %s, while %s found at %s" % (
            self.delimeters[1],
            e,
            self.delimeters[0],
            s,
        )

        # write the prefs
        cleaned_prefs = "\n".join(lines[:s] + lines[e + 1 :])
        with open(path, "w") as f:
            f.write(cleaned_prefs)
        return True

    # methods for introspection

    def summary(self, return_parts=False):
        """
        returns string summarizing profile information.
        if return_parts is true, return the (Part_name, value) list
        of tuples instead of the assembled string
        """

        parts = [("Path", self.profile)]  # profile path

        # directory tree
        parts.append(("Files", "\n%s" % mozfile.tree(self.profile)))

        # preferences
        for prefs_file in ("user.js", "prefs.js"):
            path = os.path.join(self.profile, prefs_file)
            if os.path.exists(path):

                # prefs that get their own section
                # This is currently only 'network.proxy.autoconfig_url'
                # but could be expanded to include others
                section_prefs = ["network.proxy.autoconfig_url"]
                line_length = 80
                # buffer for 80 character display:
                # length = 80 - len(key) - len(': ') - line_length_buffer
                line_length_buffer = 10
                line_length_buffer += len(": ")

                def format_value(key, value):
                    if key not in section_prefs:
                        return value
                    max_length = line_length - len(key) - line_length_buffer
                    if len(value) > max_length:
                        value = "%s..." % value[:max_length]
                    return value

                prefs = Preferences.read_prefs(path)
                if prefs:
                    prefs = dict(prefs)
                    parts.append(
                        (
                            prefs_file,
                            "\n%s"
                            % (
                                "\n".join(
                                    [
                                        "%s: %s" % (key, format_value(key, prefs[key]))
                                        for key in sorted(prefs.keys())
                                    ]
                                )
                            ),
                        )
                    )

                    # Currently hardcorded to 'network.proxy.autoconfig_url'
                    # but could be generalized, possibly with a generalized (simple)
                    # JS-parser
                    network_proxy_autoconfig = prefs.get("network.proxy.autoconfig_url")
                    if network_proxy_autoconfig and network_proxy_autoconfig.strip():
                        network_proxy_autoconfig = network_proxy_autoconfig.strip()
                        lines = network_proxy_autoconfig.replace(
                            ";", ";\n"
                        ).splitlines()
                        lines = [line.strip() for line in lines]
                        origins_string = "var origins = ["
                        origins_end = "];"
                        if origins_string in lines[0]:
                            start = lines[0].find(origins_string)
                            end = lines[0].find(origins_end, start)
                            splitline = [
                                lines[0][:start],
                                lines[0][start : start + len(origins_string) - 1],
                            ]
                            splitline.extend(
                                lines[0][start + len(origins_string) : end]
                                .replace(",", ",\n")
                                .splitlines()
                            )
                            splitline.append(lines[0][end:])
                            lines[0:1] = [i.strip() for i in splitline]
                        parts.append(
                            (
                                "Network Proxy Autoconfig, %s" % (prefs_file),
                                "\n%s" % "\n".join(lines),
                            )
                        )

        if return_parts:
            return parts

        retval = "%s\n" % (
            "\n\n".join(["[%s]: %s" % (key, value) for key, value in parts])
        )
        return retval

    def __str__(self):
        return self.summary()


class FirefoxProfile(Profile):
    """Specialized Profile subclass for Firefox"""

    preferences = {}


class ThunderbirdProfile(Profile):
    """Specialized Profile subclass for Thunderbird"""

    preferences = {
        "extensions.update.enabled": False,
        "extensions.update.notifyUser": False,
        "browser.shell.checkDefaultBrowser": False,
        "browser.tabs.warnOnClose": False,
        "browser.warnOnQuit": False,
        "browser.sessionstore.resume_from_crash": False,
        # prevents the 'new e-mail address' wizard on new profile
        "mail.provider.enabled": False,
    }


class ChromiumProfile(BaseProfile):
    preference_file_names = ("Preferences",)

    class AddonManager(list):
        def install(self, addons):
            if isinstance(addons, string_types):
                addons = [addons]
            self.extend(addons)

        @classmethod
        def is_addon(self, addon):
            # Don't include testing/profiles on Google Chrome
            return False

    def __init__(self, **kwargs):
        super(ChromiumProfile, self).__init__(**kwargs)

        if self.create_new:
            self.profile = os.path.join(self.profile, "Default")
        self._reset()

    def _reset(self):
        if not os.path.isdir(self.profile):
            os.makedirs(self.profile)

        if self._preferences:
            self.set_preferences(self._preferences)

        self.addons = self.AddonManager()
        if self._addons:
            self.addons.install(self._addons)

    def set_preferences(self, preferences, filename="Preferences", **values):
        pref_file = os.path.join(self.profile, filename)

        prefs = {}
        if os.path.isfile(pref_file):
            with open(pref_file, "r") as fh:
                prefs.update(json.load(fh))

        prefs.update(preferences)
        with open(pref_file, "w") as fh:
            prefstr = json.dumps(prefs)
            prefstr % values  # interpolate prefs with values
            if six.PY2:
                fh.write(unicode(prefstr))
            else:
                fh.write(prefstr)


class ChromeProfile(ChromiumProfile):
    # update this if Google Chrome requires more
    # specific profiles
    pass


profile_class = {
    "chrome": ChromeProfile,
    "chromium": ChromiumProfile,
    "firefox": FirefoxProfile,
    "thunderbird": ThunderbirdProfile,
}


def create_profile(app, **kwargs):
    """Create a profile given an application name.

    :param app: String name of the application to create a profile for, e.g 'firefox'.
    :param kwargs: Same as the arguments for the Profile class (optional).
    :returns: An application specific Profile instance
    :raises: NotImplementedError
    """
    cls = profile_class.get(app)

    if not cls:
        raise NotImplementedError(
            "Profiles not supported for application '{}'".format(app)
        )

    return cls(**kwargs)
