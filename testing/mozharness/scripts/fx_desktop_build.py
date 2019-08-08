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

import copy
import sys
import os

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

import mozharness.base.script as script
from mozharness.mozilla.building.buildbase import BUILD_BASE_CONFIG_OPTIONS, \
    BuildingConfig, BuildScript
from mozharness.mozilla.testing.try_tools import TryToolsMixin, try_config_options


class FxDesktopBuild(BuildScript, TryToolsMixin, object):
    def __init__(self):
        buildscript_kwargs = {
            'config_options': BUILD_BASE_CONFIG_OPTIONS + copy.deepcopy(try_config_options),
            'all_actions': [
                'get-secrets',
                'clobber',
                'build',
                'static-analysis-autotest',
                'check-test',
                'valgrind-test',
                'multi-l10n',
                'package-source',
            ],
            'require_config_file': True,
            # Default configuration
            'config': {
                'is_automation': True,
                "pgo_build": False,
                "debug_build": False,
                "pgo_platforms": ['linux', 'linux64', 'win32', 'win64'],
                # nightly stuff
                "nightly_build": False,
                # hg tool stuff
                "tools_repo": "https://hg.mozilla.org/build/tools",
                # Seed all clones with mozilla-unified. This ensures subsequent
                # jobs have a minimal `hg pull`.
                "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
                "repo_base": "https://hg.mozilla.org",
                'tooltool_url': 'https://tooltool.mozilla-releng.net/',
                "graph_selector": "/server/collect.cgi",
                # only used for make uploadsymbols
                'old_packages': [
                    "%(objdir)s/dist/firefox-*",
                    "%(objdir)s/dist/fennec*",
                    "%(objdir)s/dist/seamonkey*",
                    "%(objdir)s/dist/thunderbird*",
                    "%(objdir)s/dist/install/sea/*.exe"
                ],
                'build_resources_path': '%(upload_path)s/build_resources.json',
                'nightly_promotion_branches': ['mozilla-central', 'mozilla-aurora'],

                # try will overwrite these
                'clone_with_purge': False,
                'clone_by_revision': False,

                'virtualenv_modules': [
                    'requests==2.8.1',
                ],
                'virtualenv_path': 'venv',
                #

            },
            'ConfigClass': BuildingConfig,
        }
        super(FxDesktopBuild, self).__init__(**buildscript_kwargs)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        c = self.config
        abs_dirs = super(FxDesktopBuild, self).query_abs_dirs()
        if not c.get('app_ini_path'):
            self.fatal('"app_ini_path" is needed in your config for this '
                       'script.')

        dirs = {
            # BuildFactories in factory.py refer to a 'build' dir on the slave.
            # This contains all the source code/objdir to compile.  However,
            # there is already a build dir in mozharness for every mh run. The
            # 'build' that factory refers to I named: 'src' so
            # there is a seperation in mh.  for example, rather than having
            # '{mozharness_repo}/build/build/', I have '{
            # mozharness_repo}/build/src/'
            'abs_src_dir': os.path.join(abs_dirs['abs_work_dir'],
                                        'src'),
            'abs_obj_dir': os.path.join(abs_dirs['abs_work_dir'],
                                        'src',
                                        self._query_objdir()),
            'abs_tools_dir': os.path.join(abs_dirs['abs_work_dir'], 'tools'),
            'abs_app_ini_path': c['app_ini_path'] % {
                'obj_dir': os.path.join(abs_dirs['abs_work_dir'],
                                        'src',
                                        self._query_objdir())
            },
            'upload_path': self.config["upload_env"]["UPLOAD_PATH"],
        }
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

        # Actions {{{2

    def set_extra_try_arguments(self, action, success=None):
        """ Override unneeded method from TryToolsMixin """
        pass

    @script.PreScriptRun
    def suppress_windows_modal_dialogs(self, *args, **kwargs):
        if self._is_windows():
            # Suppress Windows modal dialogs to avoid hangs
            import ctypes
            ctypes.windll.kernel32.SetErrorMode(0x8001)


if __name__ == '__main__':
    fx_desktop_build = FxDesktopBuild()
    fx_desktop_build.run_and_exit()
