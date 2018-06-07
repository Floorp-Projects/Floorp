#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""multi_locale_build.py

This should be a mostly generic multilocale build script.
"""

import os
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.errors import MakefileErrorList
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.l10n.locales import LocalesMixin


# MultiLocaleBuild {{{1
class MultiLocaleBuild(LocalesMixin, MercurialScript):
    """ This class targets Fennec multilocale builds.
        We were considering this for potential Firefox desktop multilocale.
        Now that we have a different approach for B2G multilocale,
        it's most likely misnamed. """
    config_options = [[
        ["--locale"],
        {"action": "extend",
         "dest": "locales",
         "type": "string",
         "help": "Specify the locale(s) to repack"
         }
    ], [
        ["--objdir"],
        {"action": "store",
         "dest": "objdir",
         "type": "string",
         "default": "objdir",
         "help": "Specify the objdir"
         }
    ], [
        ["--l10n-base"],
        {"action": "store",
         "dest": "hg_l10n_base",
         "type": "string",
         "help": "Specify the L10n repo base directory"
         }
    ], [
        ["--l10n-tag"],
        {"action": "store",
         "dest": "hg_l10n_tag",
         "type": "string",
         "help": "Specify the L10n tag"
         }
    ], [
        ["--tag-override"],
        {"action": "store",
         "dest": "tag_override",
         "type": "string",
         "help": "Override the tags set for all repos"
         }
    ], [
        ["--l10n-dir"],
        {"action": "store",
         "dest": "l10n_dir",
         "type": "string",
         "default": "l10n",
         "help": "Specify the l10n dir name"
         }
    ]]

    def __init__(self, require_config_file=True):
        LocalesMixin.__init__(self)
        MercurialScript.__init__(self, config_options=self.config_options,
                                 all_actions=['pull-locale-source',
                                              'add-locales',
                                              'android-assemble-app',
                                              'package-multi',
                                              'summary'],
                                 require_config_file=require_config_file)

    def query_l10n_env(self):
        return self.query_env()

    # pull_locale_source() defined in LocalesMixin.

    def android_assemble_app(self):
        dirs = self.query_abs_dirs()

        command = 'make -C mobile/android/base android_apks'
        env = self.query_env()
        if self._process_command(command=command,
                                 cwd=dirs['abs_objdir'],
                                 env=env, error_list=MakefileErrorList):
            self.fatal("Erroring out after assembling Android APKs failed.")

    def add_locales(self):
        dirs = self.query_abs_dirs()
        locales = self.query_locales()

        for locale in locales:
            command = 'make chrome-%s L10NBASEDIR=%s' % (locale, dirs['abs_l10n_dir'])
            status = self._process_command(command=command,
                                           cwd=dirs['abs_locales_dir'],
                                           error_list=MakefileErrorList)
            if status:
                self.return_code += 1
                self.add_summary("Failed to add locale %s!" % locale,
                                 level="error")
            else:
                self.add_summary("Added locale %s successfully." % locale)

    def preflight_package_multi(self):
        dirs = self.query_abs_dirs()
        self.run_command("rm -rfv dist/fennec*", cwd=dirs['abs_objdir'])
        # bug 933290
        self.run_command(["touch", "mobile/android/installer/Makefile"], cwd=dirs['abs_objdir'])

    def package_multi(self):
        self.package(package_type='multi')

    def additional_packaging(self, package_type='en-US', env=None):
        dirs = self.query_abs_dirs()
        command = "make package-tests"
        if package_type == 'multi':
            command += " AB_CD=multi"
        self.run_command(command, cwd=dirs['abs_objdir'], env=env,
                         error_list=MakefileErrorList,
                         halt_on_failure=True)
        # TODO deal with buildsymbols

    def package(self, package_type='en-US'):
        dirs = self.query_abs_dirs()

        command = "make package"
        env = self.query_env()
        if env is None:
            # This is for Maemo, where we don't want an env for builds
            # but we do for packaging.  self.query_env() will return None.
            env = os.environ.copy()
        if package_type == 'multi':
            command += " AB_CD=multi"
            env['MOZ_CHROME_MULTILOCALE'] = "en-US " + \
                                            ' '.join(self.query_locales())
            self.info("MOZ_CHROME_MULTILOCALE is %s" % env['MOZ_CHROME_MULTILOCALE'])
        self._process_command(command=command, cwd=dirs['abs_objdir'],
                              env=env, error_list=MakefileErrorList,
                              halt_on_failure=True)
        self.additional_packaging(package_type=package_type, env=env)

    def _process_command(self, **kwargs):
        """Stub wrapper function that allows us to call scratchbox in
           MaemoMultiLocaleBuild.

        """
        return self.run_command(**kwargs)

# __main__ {{{1
if __name__ == '__main__':
    pass
