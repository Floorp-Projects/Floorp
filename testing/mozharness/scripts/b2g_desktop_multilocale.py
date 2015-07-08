#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import sys
import os

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

# import the guts
from mozharness.base.config import parse_config_file
from mozharness.base.errors import MakefileErrorList
from mozharness.base.log import ERROR, FATAL
from mozharness.base.script import BaseScript
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.l10n.locales import GaiaLocalesMixin, LocalesMixin


class B2gMultilocale(LocalesMixin, BaseScript, VCSMixin, GaiaLocalesMixin):
    """ This is a helper script that requires MercurialBuildFactory
        logic to work.  We may eventually make this a standalone
        script.

        We could inherit MercurialScript instead of BaseScript + VCSMixin
    """
    config_options = [
        [["--locale"], {
            "action": "extend",
            "dest": "locales",
            "type": "string",
            "help": "Specify the locale(s) to repack"
        }],
        [["--gaia-languages-file"], {
            "dest": "gaia_languages_file",
            "help": "languages file for gaia multilocale profile",
        }],
        [["--gecko-languages-file"], {
            "dest": "locales_file",
            "help": "languages file for gecko multilocale",
        }],
        [["--gecko-l10n-root"], {
            "dest": "hg_l10n_base",
            "help": "root location for gecko l10n repos",
        }],
        [["--gecko-l10n-base-dir"], {
            "dest": "l10n_dir",
            "help": "dir to clone gecko l10n repos into, relative to the work directory",
        }],
        [["--merge-locales"], {
            "dest": "merge_locales",
            "help": "Dummy option to keep from burning. We now always merge",
        }],
        [["--gaia-l10n-root"], {
            "dest": "gaia_l10n_root",
            "help": "root location for gaia l10n repos",
        }],
        [["--gaia-l10n-base-dir"], {
            "dest": "gaia_l10n_base_dir",
            "default": "build-gaia-l10n",
            "help": "dir to clone l10n repos into, relative to the work directory",
        }],
        [["--gaia-l10n-vcs"], {
            "dest": "gaia_l10n_vcs",
            "help": "vcs to use for gaia l10n",
        }],
    ]

    def __init__(self, require_config_file=False):
        LocalesMixin.__init__(self)
        BaseScript.__init__(self,
                            config_options=self.config_options,
                            all_actions=[
                                'pull',
                                'build',
                                'summary',
                            ],
                            require_config_file=require_config_file,

                            # Default configuration
                            config={
                                'gaia_l10n_vcs': 'hg',
                                'vcs_share_base': os.environ.get('HG_SHARE_BASE_DIR'),
                                'locales_dir': 'b2g/locales',
                                'l10n_dir': 'gecko-l10n',
                                # I forget what this was for.  Copied from the Android multilocale stuff
                                'ignore_locales': ["en-US", "multi"],
                                # This only has 2 locales in it.  We probably need files that mirror gaia's locale lists
                                # We need 2 sets of locales files because the locale names in gaia are different than gecko, e.g. 'es' vs 'es-ES'
                                # We'll need to override this for localizer buidls
                                'locales_file': 'build/b2g/locales/all-locales',
                                'mozilla_dir': 'build',
                                'objdir': 'obj-firefox',
                                'merge_locales': True,
                                'work_dir': '.',
                                'vcs_output_timeout': 600,  # 10 minutes should be enough for anyone!
                            },
                            )

    def _pre_config_lock(self, rw_config):
        super(B2gMultilocale, self)._pre_config_lock(rw_config)

        if 'pull' in self.actions:
            message = ""
            if 'gaia_languages_file' not in self.config:
                message += 'Must specify --gaia-languages-file!\n'
            if 'gaia_l10n_root' not in self.config:
                message += 'Must specify --gaia-l10n-root!\n'
            if 'locales_file' not in self.config:
                message += 'Must specify --gecko-languages-file!\n'
            if 'hg_l10n_base' not in self.config:
                message += 'Must specify --gecko-l10n-root!\n'
            if message:
                self.fatal(message)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = LocalesMixin.query_abs_dirs(self)

        c = self.config
        dirs = {
            'src': os.path.join(c['work_dir'], 'gecko'),
            'work_dir': abs_dirs['abs_work_dir'],
            'gaia_l10n_base_dir': os.path.join(abs_dirs['abs_work_dir'], self.config['gaia_l10n_base_dir']),
            'abs_compare_locales_dir': os.path.join(abs_dirs['base_work_dir'], 'compare-locales'),
        }

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    # Actions {{{2
    def pull(self):
        """ Clone gaia and gecko locale repos
        """
        languages_file = self.config['gaia_languages_file']
        l10n_base_dir = self.query_abs_dirs()['gaia_l10n_base_dir']
        l10n_config = {
            'root': self.config['gaia_l10n_root'],
            'vcs': self.config['gaia_l10n_vcs'],
        }

        self.pull_gaia_locale_source(l10n_config, parse_config_file(languages_file).keys(), l10n_base_dir)
        self.pull_locale_source()
        gecko_locales = self.query_locales()
        # populate b2g/overrides, which isn't in gecko atm
        dirs = self.query_abs_dirs()
        for locale in gecko_locales:
            self.mkdir_p(os.path.join(dirs['abs_l10n_dir'], locale, 'b2g', 'chrome', 'overrides'))
            self.copytree(os.path.join(dirs['abs_l10n_dir'], locale, 'mobile', 'overrides'),
                          os.path.join(dirs['abs_l10n_dir'], locale, 'b2g', 'chrome', 'overrides'),
                          error_level=FATAL)

    def build(self):
        """ Do the multilocale portion of the build + packaging.
        """
        dirs = self.query_abs_dirs()
        gecko_locales = self.query_locales()
        make = self.query_exe('make', return_type='string')
        env = self.query_env(
            partial_env={
                'LOCALE_MERGEDIR': dirs['abs_merge_dir'],
                'MOZ_CHROME_MULTILOCALE': 'en-US ' + ' '.join(gecko_locales),
            }
        )
        merge_env = self.query_env(
            partial_env={
                'PATH': '%(PATH)s' + os.pathsep + os.path.join(dirs['abs_compare_locales_dir'], 'scripts'),
                'PYTHONPATH': os.path.join(dirs['abs_compare_locales_dir'],
                                           'lib'),

            }
        )
        for locale in gecko_locales:
            command = make + ' merge-%s L10NBASEDIR=%s LOCALE_MERGEDIR=%s' % (locale, dirs['abs_l10n_dir'], dirs['abs_merge_dir'])
            status = self.run_command(command,
                                      cwd=dirs['abs_locales_dir'],
                                      error_list=MakefileErrorList,
                                      env=merge_env)
            command = make + ' chrome-%s L10NBASEDIR=%s LOCALE_MERGEDIR=%s' % (locale, dirs['abs_l10n_dir'], dirs['abs_merge_dir'])
            status = self.run_command(command,
                                      cwd=dirs['abs_locales_dir'],
                                      error_list=MakefileErrorList)
            if status:
                self.add_summary("Failed to add locale %s!" % locale, level=ERROR)
        command = make + ' -C app'
        self.run_command(command,
                         cwd=os.path.join(dirs['abs_objdir'], 'b2g'),
                         error_list=MakefileErrorList,
                         halt_on_failure=True)
        command = make + ' package AB_CD=multi'
        self.run_command(command, cwd=dirs['abs_objdir'],
                         error_list=MakefileErrorList, env=env,
                         halt_on_failure=True)
        command = make + ' package-tests AB_CD=multi'
        self.run_command(command, cwd=dirs['abs_objdir'],
                         error_list=MakefileErrorList, env=env,
                         halt_on_failure=True)


# main {{{1
if __name__ == '__main__':
    myScript = B2gMultilocale()
    myScript.run_and_exit()
