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

import sys
import os

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.mozilla.building.buildbase import BUILD_BASE_CONFIG_OPTIONS, \
    BuildingConfig, BuildScript


class FxDesktopBuild(BuildScript, object):
    def __init__(self):
        buildscript_kwargs = {
            'config_options': BUILD_BASE_CONFIG_OPTIONS,
            'all_actions': [
                'get-secrets',
                'clobber',
                'clone-tools',
                'checkout-sources',
                'setup-mock',
                'build',
                'upload-files',  # upload from BB to TC
                'sendchange',
                'check-test',
                'valgrind-test',
                'package-source',
                'generate-source-signing-manifest',
                'multi-l10n',
                'generate-build-stats',
                'update',
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
                'balrog_credentials_file': 'oauth.txt',
                'taskcluster_credentials_file': 'oauth.txt',
                'periodic_clobber': 168,
                # hg tool stuff
                "tools_repo": "https://hg.mozilla.org/build/tools",
                "repo_base": "https://hg.mozilla.org",
                'tooltool_url': 'https://api.pub.build.mozilla.org/tooltool/',
                "graph_selector": "/server/collect.cgi",
                # only used for make uploadsymbols
                'old_packages': [
                    "%(objdir)s/dist/firefox-*",
                    "%(objdir)s/dist/fennec*",
                    "%(objdir)s/dist/seamonkey*",
                    "%(objdir)s/dist/thunderbird*",
                    "%(objdir)s/dist/install/sea/*.exe"
                ],
                'stage_product': 'firefox',
                'platform_supports_post_upload_to_latest': True,
                'latest_mar_dir': '/pub/mozilla.org/firefox/nightly/latest-%(branch)s',
                'compare_locales_repo': 'https://hg.mozilla.org/build/compare-locales',
                'compare_locales_rev': 'RELEASE_AUTOMATION',
                'compare_locales_vcs': 'hgtool',
                'influx_credentials_file': 'oauth.txt',
                'build_resources_path': '%(abs_src_dir)s/obj-firefox/.mozbuild/build_resources.json',
                'nightly_promotion_branches': ['mozilla-central', 'mozilla-aurora'],

                # try will overwrite these
                'clone_with_purge': False,
                'clone_by_revision': False,
                'tinderbox_build_dir': None,
                'to_tinderbox_dated': True,
                'release_to_try_builds': False,
                'include_post_upload_builddir': False,
                'use_clobberer': True,

                'stage_username': 'ffxbld',
                'stage_ssh_key': 'ffxbld_rsa',
                'virtualenv_modules': [
                    'requests==2.8.1',
                    'PyHawk-with-a-single-extra-commit==0.1.5',
                    'taskcluster==0.0.26',
                ],
                'virtualenv_path': 'venv',
                #

            },
            'ConfigClass': BuildingConfig,
        }
        super(FxDesktopBuild, self).__init__(**buildscript_kwargs)

    def _pre_config_lock(self, rw_config):
        """grab buildbot props if we are running this in automation"""
        super(FxDesktopBuild, self)._pre_config_lock(rw_config)
        c = self.config
        if c['is_automation']:
            # parse buildbot config and add it to self.config
            self.info("We are running this in buildbot, grab the build props")
            self.read_buildbot_config()
            ###
            if c.get('stage_platform'):
                platform_for_log_url = c['stage_platform']
                if c.get('pgo_build'):
                    platform_for_log_url += '-pgo'
                # postrun.py uses stage_platform buildbot prop as part of the log url
                self.set_buildbot_property('stage_platform',
                                           platform_for_log_url,
                                           write_to_file=True)
            else:
                self.fatal("'stage_platform' not determined and is required in your config")


    # helpers

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
            'compare_locales_dir': os.path.join(abs_dirs['abs_work_dir'], 'compare-locales'),
        }
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

        # Actions {{{2
        # read_buildbot_config in BuildingMixin
        # clobber in BuildingMixin -> PurgeMixin
        # if Linux config:
        # reset_mock in BuildingMixing -> MockMixin
        # setup_mock in BuildingMixing (overrides MockMixin.mock_setup)


if __name__ == '__main__':
    fx_desktop_build = FxDesktopBuild()
    fx_desktop_build.run_and_exit()
