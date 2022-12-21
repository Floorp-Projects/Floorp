# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import shutil
import tempfile
from pathlib import Path

from condprof.client import ProfileNotFoundError, get_profile
from condprof.util import get_current_platform
from mozprofile import create_profile
from mozprofile.prefs import Preferences

from mozperftest.layers import Layer

HERE = os.path.dirname(__file__)


class Profile(Layer):
    name = "profile"
    activated = True
    arguments = {
        "directory": {"type": str, "default": None, "help": "Profile to use"},
        "user-js": {"type": str, "default": None, "help": "Custom user.js"},
        "conditioned": {
            "action": "store_true",
            "default": False,
            "help": "Use a conditioned profile.",
        },
        "conditioned-scenario": {
            "type": str,
            "default": "settled",
            "help": "Conditioned scenario to use",
        },
        "conditioned-platform": {
            "type": str,
            "default": None,
            "help": "Conditioned platform to use (use local by default)",
        },
        "conditioned-project": {
            "type": str,
            "default": "mozilla-central",
            "help": "Conditioned project",
            "choices": ["try", "mozilla-central"],
        },
    }

    def __init__(self, env, mach_cmd):
        super(Profile, self).__init__(env, mach_cmd)
        self._created_dirs = []

    def setup(self):
        pass

    def _cleanup(self):
        pass

    def _get_conditioned_profile(self):
        platform = self.get_arg("conditioned-platform")
        if platform is None:
            platform = get_current_platform()
        scenario = self.get_arg("conditioned-scenario")
        project = self.get_arg("conditioned-project")
        alternate_project = "mozilla-central" if project != "mozilla-central" else "try"

        temp_dir = tempfile.mkdtemp()
        try:
            condprof = get_profile(temp_dir, platform, scenario, repo=project)
        except ProfileNotFoundError:
            condprof = get_profile(temp_dir, platform, scenario, repo=alternate_project)
        except Exception:
            raise

        # now get the full directory path to our fetched conditioned profile
        condprof = Path(temp_dir, condprof)
        if not condprof.exists():
            raise OSError(str(condprof))

        return condprof

    def run(self, metadata):
        # using a conditioned profile
        if self.get_arg("conditioned"):
            profile_dir = self._get_conditioned_profile()
            self.set_arg("profile-directory", str(profile_dir))
            self._created_dirs.append(str(profile_dir))
            return metadata

        if self.get_arg("directory") is not None:
            # no need to create one or load a conditioned one
            return metadata

        # fresh profile
        profile = create_profile(app="firefox")

        # mozprofile.Profile.__del__ silently deletes the profile
        # it creates in a non-deterministic time (garbage collected) by
        # calling cleanup. We override this silly behavior here.
        profile.cleanup = self._cleanup

        prefs = metadata.get_options("browser_prefs")

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
