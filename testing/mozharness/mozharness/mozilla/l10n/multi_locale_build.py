#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""multi_locale_build.py

This should be a mostly generic multilocale build script.
"""

from copy import deepcopy
import os
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.errors import MakefileErrorList, SSHErrorList
from mozharness.base.log import FATAL
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
        ["--merge-locales"],
        {"action": "store_true",
         "dest": "merge_locales",
         "default": False,
         "help": "Use default [en-US] if there are missing strings"
         }
    ], [
        ["--no-merge-locales"],
        {"action": "store_false",
         "dest": "merge_locales",
         "help": "Do not allow missing strings"
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
        ["--user-repo-override"],
        {"action": "store",
         "dest": "user_repo_override",
         "type": "string",
         "help": "Override the user repo path for all repos"
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
                                 all_actions=['clobber', 'pull-build-source',
                                              'pull-locale-source',
                                              'build', 'package-en-US',
                                              'upload-en-US',
                                              'backup-objdir',
                                              'restore-objdir',
                                              'add-locales', 'package-multi',
                                              'upload-multi', 'summary'],
                                 require_config_file=require_config_file)

    def query_l10n_env(self):
        return self.query_env()

    def clobber(self):
        c = self.config
        if c['work_dir'] != '.':
            path = os.path.join(c['base_work_dir'], c['work_dir'])
            if os.path.exists(path):
                self.rmtree(path, error_level=FATAL)
        else:
            self.info("work_dir is '.'; skipping for now.")

    def pull_build_source(self):
        c = self.config
        repos = []
        replace_dict = {}
        # Replace %(user_repo_override)s with c['user_repo_override']
        if c.get("user_repo_override"):
            replace_dict['user_repo_override'] = c['user_repo_override']
            for repo_dict in deepcopy(c['repos']):
                repo_dict['repo'] = repo_dict['repo'] % replace_dict
                repos.append(repo_dict)
        else:
            repos = c['repos']
        self.vcs_checkout_repos(repos, tag_override=c.get('tag_override'))

    # pull_locale_source() defined in LocalesMixin.

    def build(self):
        c = self.config
        dirs = self.query_abs_dirs()
        self.copyfile(os.path.join(dirs['abs_work_dir'], c['mozconfig']),
                      os.path.join(dirs['abs_mozilla_dir'], 'mozconfig'),
                      error_level=FATAL)
        command = "make -f client.mk build"
        env = self.query_env()
        if self._process_command(command=command,
                                 cwd=dirs['abs_mozilla_dir'],
                                 env=env, error_list=MakefileErrorList):
            self.fatal("Erroring out after the build failed.")

    def add_locales(self):
        c = self.config
        dirs = self.query_abs_dirs()
        locales = self.query_locales()

        for locale in locales:
            self.run_compare_locales(locale, halt_on_failure=True)
            command = 'make chrome-%s L10NBASEDIR=%s' % (locale, dirs['abs_l10n_dir'])
            if c['merge_locales']:
                command += " LOCALE_MERGEDIR=%s" % dirs['abs_merge_dir'].replace(os.sep, '/')
            status = self._process_command(command=command,
                                           cwd=dirs['abs_locales_dir'],
                                           error_list=MakefileErrorList)
            if status:
                self.return_code += 1
                self.add_summary("Failed to add locale %s!" % locale,
                                 level="error")
            else:
                self.add_summary("Added locale %s successfully." % locale)

    def package_en_US(self):
        self.package(package_type='en-US')

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

    def upload_en_US(self):
        # TODO
        self.info("Not written yet.")

    def backup_objdir(self):
        dirs = self.query_abs_dirs()
        if not os.path.isdir(dirs['abs_objdir']):
            self.warning("%s doesn't exist! Skipping..." % dirs['abs_objdir'])
            return
        rsync = self.query_exe('rsync')
        backup_dir = '%s-bak' % dirs['abs_objdir']
        self.rmtree(backup_dir)
        self.mkdir_p(backup_dir)
        self.run_command([rsync, '-a', '--delete', '--partial',
                          '%s/' % dirs['abs_objdir'],
                          '%s/' % backup_dir],
                         error_list=SSHErrorList)

    def restore_objdir(self):
        dirs = self.query_abs_dirs()
        rsync = self.query_exe('rsync')
        backup_dir = '%s-bak' % dirs['abs_objdir']
        if not os.path.isdir(dirs['abs_objdir']) or not os.path.isdir(backup_dir):
            self.warning("Both %s and %s need to exist to restore the objdir! Skipping..." % (dirs['abs_objdir'], backup_dir))
            return
        self.run_command([rsync, '-a', '--delete', '--partial',
                          '%s/' % backup_dir,
                          '%s/' % dirs['abs_objdir']],
                         error_list=SSHErrorList)

    def upload_multi(self):
        # TODO
        self.info("Not written yet.")

    def _process_command(self, **kwargs):
        """Stub wrapper function that allows us to call scratchbox in
           MaemoMultiLocaleBuild.

        """
        return self.run_command(**kwargs)

# __main__ {{{1
if __name__ == '__main__':
    pass
