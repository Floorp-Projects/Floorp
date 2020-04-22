# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import shutil

from mozperftest.layers import Layer
from mozprofile import create_profile
from mozprofile.prefs import Preferences


HERE = os.path.dirname(__file__)


class Profile(Layer):
    name = "profile"
    activated = True
    arguments = {
        "directory": {"type": str, "default": None, "help": "Profile to use"},
        "user-js": {"type": str, "default": None, "help": "Custom user.js"},
    }

    def __init__(self, env, mach_cmd):
        super(Profile, self).__init__(env, mach_cmd)
        self._created_dirs = []

    def setup(self):
        pass

    def _cleanup(self):
        pass

    def __call__(self, metadata):
        if self.get_arg("directory") is not None:
            # no need to create one or load a conditioned one
            return

        # XXX we'll use conditioned profiles later
        profile = create_profile(app="firefox")

        # mozprofile.Profile.__del__ silently deletes the profile
        # it creates in a non-deterministic time (garbage collected) by
        # calling cleanup. We override this silly behavior here.
        profile.cleanup = self._cleanup

        prefs = metadata.get_browser_prefs()

        if prefs == {}:
            prefs["mozperftest"] = "true"

        # apply custom user prefs if any
        user_js = self.get_arg("user-js")
        if user_js is not None:
            self.info("Applying use prefs from %s" % user_js)
            default_prefs = dict(Preferences.read_prefs(user_js))
            prefs.update(default_prefs)

        profile.set_preferences(prefs)
        self.info("Created profile at %s" % profile.profile)
        self._created_dirs.append(profile.profile)
        self.set_arg("profile-directory", profile.profile)
        return metadata

    def teardown(self):
        for dir in self._created_dirs:
            if os.path.exists(dir):
                shutil.rmtree(dir)
