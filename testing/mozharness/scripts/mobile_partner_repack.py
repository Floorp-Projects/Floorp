#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""mobile_partner_repack.py

"""

from copy import deepcopy
import os
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import ZipErrorList
from mozharness.base.log import FATAL
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.l10n.locales import LocalesMixin
from mozharness.mozilla.release import ReleaseMixin
from mozharness.mozilla.signing import MobileSigningMixin

SUPPORTED_PLATFORMS = ["android"]


# MobilePartnerRepack {{{1
class MobilePartnerRepack(LocalesMixin, ReleaseMixin, MobileSigningMixin,
                          TransferMixin, MercurialScript):
    config_options = [[
        ['--locale', ],
        {"action": "extend",
         "dest": "locales",
         "type": "string",
         "help": "Specify the locale(s) to repack"
         }
    ], [
        ['--partner', ],
        {"action": "extend",
         "dest": "partners",
         "type": "string",
         "help": "Specify the partner(s) to repack"
         }
    ], [
        ['--locales-file', ],
        {"action": "store",
         "dest": "locales_file",
         "type": "string",
         "help": "Specify a json file to determine which locales to repack"
         }
    ], [
        ['--tag-override', ],
        {"action": "store",
         "dest": "tag_override",
         "type": "string",
         "help": "Override the tags set for all repos"
         }
    ], [
        ['--platform', ],
        {"action": "extend",
         "dest": "platforms",
         "type": "choice",
         "choices": SUPPORTED_PLATFORMS,
         "help": "Specify the platform(s) to repack"
         }
    ], [
        ['--user-repo-override', ],
        {"action": "store",
         "dest": "user_repo_override",
         "type": "string",
         "help": "Override the user repo path for all repos"
         }
    ], [
        ['--release-config-file', ],
        {"action": "store",
         "dest": "release_config_file",
         "type": "string",
         "help": "Specify the release config file to use"
         }
    ], [
        ['--version', ],
        {"action": "store",
         "dest": "version",
         "type": "string",
         "help": "Specify the current version"
         }
    ], [
        ['--buildnum', ],
        {"action": "store",
         "dest": "buildnum",
         "type": "int",
         "default": 1,
         "metavar": "INT",
         "help": "Specify the current release build num (e.g. build1, build2)"
         }
    ]]

    def __init__(self, require_config_file=True):
        self.release_config = {}
        LocalesMixin.__init__(self)
        MercurialScript.__init__(
            self,
            config_options=self.config_options,
            all_actions=[
                "passphrase",
                "clobber",
                "pull",
                "download",
                "repack",
                "upload-unsigned-bits",
                "sign",
                "upload-signed-bits",
                "summary",
            ],
            require_config_file=require_config_file
        )

    # Helper methods {{{2
    def add_failure(self, platform, locale, **kwargs):
        s = "%s:%s" % (platform, locale)
        if 'message' in kwargs:
            kwargs['message'] = kwargs['message'] % {'platform': platform, 'locale': locale}
        super(MobilePartnerRepack, self).add_failure(s, **kwargs)

    def query_failure(self, platform, locale):
        s = "%s:%s" % (platform, locale)
        return super(MobilePartnerRepack, self).query_failure(s)

    # Actions {{{2

    def pull(self):
        c = self.config
        dirs = self.query_abs_dirs()
        repos = []
        replace_dict = {}
        if c.get("user_repo_override"):
            replace_dict['user_repo_override'] = c['user_repo_override']
            # deepcopy() needed because of self.config lock bug :(
            for repo_dict in deepcopy(c['repos']):
                repo_dict['repo'] = repo_dict['repo'] % replace_dict
                repos.append(repo_dict)
        else:
            repos = c['repos']
        self.vcs_checkout_repos(repos, parent_dir=dirs['abs_work_dir'],
                                tag_override=c.get('tag_override'))

    def download(self):
        c = self.config
        rc = self.query_release_config()
        dirs = self.query_abs_dirs()
        locales = self.query_locales()
        replace_dict = {
            'buildnum': rc['buildnum'],
            'version': rc['version'],
        }
        success_count = total_count = 0
        for platform in c['platforms']:
            base_installer_name = c['installer_base_names'][platform]
            base_url = c['download_base_url'] + '/' + \
                c['download_unsigned_base_subdir'] + '/' + \
                base_installer_name
            replace_dict['platform'] = platform
            for locale in locales:
                replace_dict['locale'] = locale
                url = base_url % replace_dict
                installer_name = base_installer_name % replace_dict
                parent_dir = '%s/original/%s/%s' % (dirs['abs_work_dir'],
                                                    platform, locale)
                file_path = '%s/%s' % (parent_dir, installer_name)
                self.mkdir_p(parent_dir)
                total_count += 1
                if not self.download_file(url, file_path):
                    self.add_failure(platform, locale,
                                     message="Unable to download %(platform)s:%(locale)s installer!")
                else:
                    success_count += 1
        self.summarize_success_count(success_count, total_count,
                                     message="Downloaded %d of %d installers successfully.")

    def _repack_apk(self, partner, orig_path, repack_path):
        """ Repack the apk with a partner update channel.
        Returns True for success, None for failure
        """
        dirs = self.query_abs_dirs()
        zip_bin = self.query_exe("zip")
        unzip_bin = self.query_exe("unzip")
        file_name = os.path.basename(orig_path)
        tmp_dir = os.path.join(dirs['abs_work_dir'], 'tmp')
        tmp_file = os.path.join(tmp_dir, file_name)
        tmp_prefs_dir = os.path.join(tmp_dir, 'defaults', 'pref')
        # Error checking for each step.
        # Ignoring the mkdir_p()s since the subsequent copyfile()s will
        # error out if unsuccessful.
        if self.rmtree(tmp_dir):
            return
        self.mkdir_p(tmp_prefs_dir)
        if self.copyfile(orig_path, tmp_file):
            return
        if self.write_to_file(os.path.join(tmp_prefs_dir, 'partner.js'),
                              'pref("app.partner.%s", "%s");' % (partner, partner)
                              ) is None:
            return
        if self.run_command([unzip_bin, '-q', file_name, 'omni.ja'],
                            error_list=ZipErrorList,
                            return_type='num_errors',
                            cwd=tmp_dir):
            self.error("Can't extract omni.ja from %s!" % file_name)
            return
        if self.run_command([zip_bin, '-9r', 'omni.ja',
                             'defaults/pref/partner.js'],
                            error_list=ZipErrorList,
                            return_type='num_errors',
                            cwd=tmp_dir):
            self.error("Can't add partner.js to omni.ja!")
            return
        if self.run_command([zip_bin, '-9r', file_name, 'omni.ja'],
                            error_list=ZipErrorList,
                            return_type='num_errors',
                            cwd=tmp_dir):
            self.error("Can't re-add omni.ja to %s!" % file_name)
            return
        if self.unsign_apk(tmp_file):
            return
        repack_dir = os.path.dirname(repack_path)
        self.mkdir_p(repack_dir)
        if self.copyfile(tmp_file, repack_path):
            return
        return True

    def repack(self):
        c = self.config
        rc = self.query_release_config()
        dirs = self.query_abs_dirs()
        locales = self.query_locales()
        success_count = total_count = 0
        for platform in c['platforms']:
            for locale in locales:
                installer_name = c['installer_base_names'][platform] % {'version': rc['version'], 'locale': locale}
                if self.query_failure(platform, locale):
                    self.warning("%s:%s had previous issues; skipping!" % (platform, locale))
                    continue
                original_path = '%s/original/%s/%s/%s' % (dirs['abs_work_dir'], platform, locale, installer_name)
                for partner in c['partner_config'].keys():
                    repack_path = '%s/unsigned/partner-repacks/%s/%s/%s/%s' % (dirs['abs_work_dir'], partner, platform, locale, installer_name)
                    total_count += 1
                    if self._repack_apk(partner, original_path, repack_path):
                        success_count += 1
                    else:
                        self.add_failure(platform, locale,
                                         message="Unable to repack %(platform)s:%(locale)s installer!")
        self.summarize_success_count(success_count, total_count,
                                     message="Repacked %d of %d installers successfully.")

    def _upload(self, dir_name="unsigned/partner-repacks"):
        c = self.config
        dirs = self.query_abs_dirs()
        local_path = os.path.join(dirs['abs_work_dir'], dir_name)
        rc = self.query_release_config()
        replace_dict = {
            'buildnum': rc['buildnum'],
            'version': rc['version'],
        }
        remote_path = '%s/%s' % (c['ftp_upload_base_dir'] % replace_dict, dir_name)
        if self.rsync_upload_directory(local_path, c['ftp_ssh_key'],
                                       c['ftp_user'], c['ftp_server'],
                                       remote_path):
            self.return_code += 1

    def upload_unsigned_bits(self):
        self._upload()

    # passphrase() in AndroidSigningMixin
    # verify_passphrases() in AndroidSigningMixin

    def preflight_sign(self):
        if 'passphrase' not in self.actions:
            self.passphrase()
            self.verify_passphrases()

    def sign(self):
        c = self.config
        rc = self.query_release_config()
        dirs = self.query_abs_dirs()
        locales = self.query_locales()
        success_count = total_count = 0
        for platform in c['platforms']:
            for locale in locales:
                installer_name = c['installer_base_names'][platform] % {'version': rc['version'], 'locale': locale}
                if self.query_failure(platform, locale):
                    self.warning("%s:%s had previous issues; skipping!" % (platform, locale))
                    continue
                for partner in c['partner_config'].keys():
                    unsigned_path = '%s/unsigned/partner-repacks/%s/%s/%s/%s' % (dirs['abs_work_dir'], partner, platform, locale, installer_name)
                    signed_dir = '%s/partner-repacks/%s/%s/%s' % (dirs['abs_work_dir'], partner, platform, locale)
                    signed_path = "%s/%s" % (signed_dir, installer_name)
                    total_count += 1
                    self.info("Signing %s %s." % (platform, locale))
                    if not os.path.exists(unsigned_path):
                        self.error("Missing apk %s!" % unsigned_path)
                        continue
                    if self.sign_apk(unsigned_path, c['keystore'],
                                     self.store_passphrase, self.key_passphrase,
                                     c['key_alias']) != 0:
                        self.add_summary("Unable to sign %s:%s apk!" % (platform, locale), level=FATAL)
                    else:
                        self.mkdir_p(signed_dir)
                        if self.align_apk(unsigned_path, signed_path):
                            self.add_failure(platform, locale,
                                             message="Unable to align %(platform)s%(locale)s apk!")
                            self.rmtree(signed_dir)
                        else:
                            success_count += 1
        self.summarize_success_count(success_count, total_count,
                                     message="Signed %d of %d apks successfully.")

    # TODO verify signatures.

    def upload_signed_bits(self):
        self._upload(dir_name="partner-repacks")


# main {{{1
if __name__ == '__main__':
    mobile_partner_repack = MobilePartnerRepack()
    mobile_partner_repack.run_and_exit()
