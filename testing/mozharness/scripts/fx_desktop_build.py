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
import pprint
import sys
import os

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.mozilla.building.buildbase import BUILD_BASE_CONFIG_OPTIONS, \
    BuildingConfig, BuildOptionParser, BuildScript
from mozharness.base.config import parse_config_file
from mozharness.mozilla.testing.try_tools import TryToolsMixin, try_config_options


class FxDesktopBuild(BuildScript, TryToolsMixin, object):
    def __init__(self):
        buildscript_kwargs = {
            'config_options': BUILD_BASE_CONFIG_OPTIONS + copy.deepcopy(try_config_options),
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

        if self.try_message_has_flag('artifact'):
            self.info('Artifact build requested in try syntax.')
            variant = 'artifact'
            if c.get('build_variant') in ['debug', 'cross-debug']:
                variant = 'debug-artifact'
            self._update_build_variant(rw_config, variant)

    # helpers
    def _update_build_variant(self, rw_config, variant='artifact'):
        """ Intended for use in _pre_config_lock """
        c = self.config
        variant_cfg_path, _ = BuildOptionParser.find_variant_cfg_path(
            '--custom-build-variant-cfg',
            variant,
            rw_config.config_parser
        )
        if not variant_cfg_path:
            self.fatal('Could not find appropriate config file for variant %s' % variant)
        # Update other parts of config to keep dump-config accurate
        # Only dump-config is affected because most config info is set during
        # initial parsing
        variant_cfg_dict = parse_config_file(variant_cfg_path)
        rw_config.all_cfg_files_and_dicts.append((variant_cfg_path, variant_cfg_dict))
        c.update({
            'build_variant': variant,
            'config_files': c['config_files'] + [variant_cfg_path]
        })

        self.info("Updating self.config with the following from {}:".format(variant_cfg_path))
        self.info(pprint.pformat(variant_cfg_dict))
        c.update(variant_cfg_dict)
        c['forced_artifact_build'] = True
        # Bug 1231320 adds MOZHARNESS_ACTIONS in TaskCluster tasks to override default_actions
        # We don't want that when forcing an artifact build.
        if rw_config.volatile_config['actions']:
            self.info("Updating volatile_config to include default_actions "
                      "from {}.".format(variant_cfg_path))
            # add default actions in correct order
            combined_actions = []
            for a in rw_config.all_actions:
                if a in c['default_actions'] or a in rw_config.volatile_config['actions']:
                    combined_actions.append(a)
            rw_config.volatile_config['actions'] = combined_actions
            self.info("Actions in volatile_config are now: {}".format(
                rw_config.volatile_config['actions'])
            )
        # replace rw_config as well to set actions as in BaseScript
        rw_config.set_config(c, overwrite=True)
        rw_config.update_actions()
        self.actions = tuple(rw_config.actions)
        self.all_actions = tuple(rw_config.all_actions)


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

    def set_extra_try_arguments(self, action, success=None):
        """ Override unneeded method from TryToolsMixin """
        pass

if __name__ == '__main__':
    fx_desktop_build = FxDesktopBuild()
    fx_desktop_build.run_and_exit()
