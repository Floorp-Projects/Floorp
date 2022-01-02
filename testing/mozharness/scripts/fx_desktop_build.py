#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""fx_desktop_build.py.

script harness to build nightly firefox within Mozilla's build environment
and developer machines alike

author: Jordan Lund

"""

from __future__ import absolute_import
import sys
import os

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

import mozharness.base.script as script
from mozharness.mozilla.building.buildbase import (
    BUILD_BASE_CONFIG_OPTIONS,
    BuildingConfig,
    BuildScript,
)


class FxDesktopBuild(BuildScript, object):
    def __init__(self):
        buildscript_kwargs = {
            "config_options": BUILD_BASE_CONFIG_OPTIONS,
            "all_actions": [
                "get-secrets",
                "clobber",
                "build",
                "static-analysis-autotest",
                "valgrind-test",
                "multi-l10n",
                "package-source",
            ],
            "require_config_file": True,
            # Default configuration
            "config": {
                "is_automation": True,
                "debug_build": False,
                # nightly stuff
                "nightly_build": False,
                # Seed all clones with mozilla-unified. This ensures subsequent
                # jobs have a minimal `hg pull`.
                "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
                "repo_base": "https://hg.mozilla.org",
                "build_resources_path": "%(upload_path)s/build_resources.json",
                "nightly_promotion_branches": ["mozilla-central", "mozilla-aurora"],
                # try will overwrite these
                "clone_with_purge": False,
                "clone_by_revision": False,
                "virtualenv_modules": [
                    "requests==2.8.1",
                ],
                "virtualenv_path": "venv",
            },
            "ConfigClass": BuildingConfig,
        }
        super(FxDesktopBuild, self).__init__(**buildscript_kwargs)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(FxDesktopBuild, self).query_abs_dirs()

        dirs = {
            # BuildFactories in factory.py refer to a 'build' dir on the slave.
            # This contains all the source code/objdir to compile.  However,
            # there is already a build dir in mozharness for every mh run. The
            # 'build' that factory refers to I named: 'src' so
            # there is a seperation in mh.  for example, rather than having
            # '{mozharness_repo}/build/build/', I have '{
            # mozharness_repo}/build/src/'
            "abs_obj_dir": os.path.join(abs_dirs["abs_work_dir"], self._query_objdir()),
            "upload_path": self.config["upload_env"]["UPLOAD_PATH"],
        }
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

        # Actions {{{2

    @script.PreScriptRun
    def suppress_windows_modal_dialogs(self, *args, **kwargs):
        if self._is_windows():
            # Suppress Windows modal dialogs to avoid hangs
            import ctypes

            ctypes.windll.kernel32.SetErrorMode(0x8001)


if __name__ == "__main__":
    fx_desktop_build = FxDesktopBuild()
    fx_desktop_build.run_and_exit()
