#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
import os
import json

from fx_desktop_build import FxDesktopBuild
from mozharness.base.config import parse_config_file
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.l10n.locales import GaiaLocalesMixin


class B2GDesktopBuild(FxDesktopBuild, GaiaLocalesMixin, VCSMixin, object):
    def _checkout_gaia(self):
        # Checkout gaia; read gaia.json
        try:
            dirs = self.query_abs_dirs()
            gaia_json_path = os.path.join(dirs['abs_src_dir'], 'b2g', 'config', 'gaia.json')
            self.info("loading %s" % gaia_json_path)
            gaia_json = json.load(open(gaia_json_path))
            self.debug("got %s" % gaia_json)
            gaia_dir = os.path.join(dirs['abs_src_dir'], 'gaia')

            vcs_checkout_kwargs = {
                'vcs': 'gittool',
                'repo': gaia_json['git']['remote'],
                'revision': gaia_json['git']['git_revision'],
                'dest': gaia_dir,
            }

            gaia_rev = self.vcs_checkout(**vcs_checkout_kwargs)
            self.set_buildbot_property('gaia_revision', gaia_rev, write_to_file=True)
        except Exception:
            self.exception("failed to checkout gaia")
            raise

    def _checkout_gaia_l10n(self):
        # Checkout gaia l10n
        try:
            dirs = self.query_abs_dirs()
            config_json_path = os.path.join(dirs['abs_src_dir'], 'b2g', 'config', 'desktop', 'config.json')
            self.info("loading %s" % config_json_path)
            config_json = json.load(open(config_json_path))
            self.debug("got %s" % config_json)
            l10n_config = config_json['gaia']['l10n']

            languages_file = os.path.join(dirs['abs_src_dir'], 'gaia/locales/languages_all.json')
            l10n_base_dir = os.path.join(dirs['abs_work_dir'], 'build-gaia-l10n')

            # Setup the environment for the gaia build system to find the locales
            env = self.query_env()
            env['LOCALE_BASEDIR'] = l10n_base_dir
            env['LOCALES_FILE'] = languages_file

            self.pull_gaia_locale_source(l10n_config, parse_config_file(languages_file).keys(), l10n_base_dir)

        except Exception:
            self.exception("failed to clone gaia l10n repos")
            raise

    def _checkout_source(self):
        super(B2GDesktopBuild, self)._checkout_source()

        self._checkout_gaia()
        self._checkout_gaia_l10n()


if __name__ == '__main__':
    b2g_desktop_build = B2GDesktopBuild()
    b2g_desktop_build.run_and_exit()
