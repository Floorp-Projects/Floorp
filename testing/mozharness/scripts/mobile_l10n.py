#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""mobile_l10n.py

This currently supports nightly and release single locale repacks for
Android.  This also creates nightly updates.
"""

from copy import deepcopy
import os
import re
import subprocess
import sys

try:
    import simplejson as json
    assert json
except ImportError:
    import json

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import BaseErrorList, MakefileErrorList
from mozharness.base.log import OutputParser
from mozharness.base.transfer import TransferMixin
from mozharness.mozilla.buildbot import BuildbotMixin
from mozharness.mozilla.purge import PurgeMixin
from mozharness.mozilla.release import ReleaseMixin
from mozharness.mozilla.signing import MobileSigningMixin
from mozharness.mozilla.tooltool import TooltoolMixin
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.l10n.locales import LocalesMixin
from mozharness.mozilla.mock import MockMixin
from mozharness.mozilla.updates.balrog import BalrogMixin


# MobileSingleLocale {{{1
class MobileSingleLocale(MockMixin, LocalesMixin, ReleaseMixin,
                         MobileSigningMixin, TransferMixin, TooltoolMixin,
                         BuildbotMixin, PurgeMixin, MercurialScript, BalrogMixin):
    config_options = [[
        ['--locale', ],
        {"action": "extend",
         "dest": "locales",
         "type": "string",
         "help": "Specify the locale(s) to sign and update"
         }
    ], [
        ['--locales-file', ],
        {"action": "store",
         "dest": "locales_file",
         "type": "string",
         "help": "Specify a file to determine which locales to sign and update"
         }
    ], [
        ['--tag-override', ],
        {"action": "store",
         "dest": "tag_override",
         "type": "string",
         "help": "Override the tags set for all repos"
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
        ['--key-alias', ],
        {"action": "store",
         "dest": "key_alias",
         "type": "choice",
         "default": "nightly",
         "choices": ["nightly", "release"],
         "help": "Specify the signing key alias"
         }
    ], [
        ['--this-chunk', ],
        {"action": "store",
         "dest": "this_locale_chunk",
         "type": "int",
         "help": "Specify which chunk of locales to run"
         }
    ], [
        ['--total-chunks', ],
        {"action": "store",
         "dest": "total_locale_chunks",
         "type": "int",
         "help": "Specify the total number of chunks of locales"
         }
    ]]

    def __init__(self, require_config_file=True):
        LocalesMixin.__init__(self)
        MercurialScript.__init__(
            self,
            config_options=self.config_options,
            all_actions=[
                "clobber",
                "pull",
                "list-locales",
                "setup",
                "repack",
                "upload-repacks",
                "submit-to-balrog",
                "summary",
            ],
            require_config_file=require_config_file
        )
        self.base_package_name = None
        self.buildid = None
        self.make_ident_output = None
        self.repack_env = None
        self.revision = None
        self.upload_env = None
        self.version = None
        self.upload_urls = {}
        self.locales_property = {}

    # Helper methods {{{2
    def query_repack_env(self):
        if self.repack_env:
            return self.repack_env
        c = self.config
        replace_dict = {}
        if c.get('release_config_file'):
            rc = self.query_release_config()
            replace_dict = {
                'version': rc['version'],
                'buildnum': rc['buildnum']
            }
        repack_env = self.query_env(partial_env=c.get("repack_env"),
                                    replace_dict=replace_dict)
        if c.get('base_en_us_binary_url') and c.get('release_config_file'):
            rc = self.query_release_config()
            repack_env['EN_US_BINARY_URL'] = c['base_en_us_binary_url'] % replace_dict
        if 'MOZ_SIGNING_SERVERS' in os.environ:
            repack_env['MOZ_SIGN_CMD'] = subprocess.list2cmdline(self.query_moz_sign_cmd(formats='jar'))
        self.repack_env = repack_env
        return self.repack_env

    def query_upload_env(self):
        if self.upload_env:
            return self.upload_env
        c = self.config
        buildid = self.query_buildid()
        version = self.query_version()
        upload_env = self.query_env(partial_env=c.get("upload_env"),
                                    replace_dict={'buildid': buildid,
                                                  'version': version})
        if 'MOZ_SIGNING_SERVERS' in os.environ:
            upload_env['MOZ_SIGN_CMD'] = subprocess.list2cmdline(self.query_moz_sign_cmd())
        self.upload_env = upload_env
        return self.upload_env

    def _query_make_ident_output(self):
        """Get |make ident| output from the objdir.
        Only valid after setup is run.
        """
        if self.make_ident_output:
            return self.make_ident_output
        env = self.query_repack_env()
        dirs = self.query_abs_dirs()
        output = self.get_output_from_command_m(["make", "ident"],
                                                cwd=dirs['abs_locales_dir'],
                                                env=env,
                                                silent=True,
                                                halt_on_failure=True)
        parser = OutputParser(config=self.config, log_obj=self.log_obj,
                              error_list=MakefileErrorList)
        parser.add_lines(output)
        self.make_ident_output = output
        return output

    def query_buildid(self):
        """Get buildid from the objdir.
        Only valid after setup is run.
        """
        if self.buildid:
            return self.buildid
        r = re.compile("buildid (\d+)")
        output = self._query_make_ident_output()
        for line in output.splitlines():
            m = r.match(line)
            if m:
                self.buildid = m.groups()[0]
        return self.buildid

    def query_revision(self):
        """Get revision from the objdir.
        Only valid after setup is run.
        """
        if self.revision:
            return self.revision
        r = re.compile(r"gecko_revision ([0-9a-f]{12}\+?)")
        output = self._query_make_ident_output()
        for line in output.splitlines():
            m = r.match(line)
            if m:
                self.revision = m.groups()[0]
        return self.revision

    def _query_make_variable(self, variable, make_args=None):
        make = self.query_exe('make')
        env = self.query_repack_env()
        dirs = self.query_abs_dirs()
        if make_args is None:
            make_args = []
        # TODO error checking
        output = self.get_output_from_command_m(
            [make, "echo-variable-%s" % variable] + make_args,
            cwd=dirs['abs_locales_dir'], silent=True,
            env=env
        )
        parser = OutputParser(config=self.config, log_obj=self.log_obj,
                              error_list=MakefileErrorList)
        parser.add_lines(output)
        return output.strip()

    def query_base_package_name(self):
        """Get the package name from the objdir.
        Only valid after setup is run.
        """
        if self.base_package_name:
            return self.base_package_name
        self.base_package_name = self._query_make_variable(
            "PACKAGE",
            make_args=['AB_CD=%(locale)s']
        )
        return self.base_package_name

    def query_version(self):
        """Get the package name from the objdir.
        Only valid after setup is run.
        """
        if self.version:
            return self.version
        c = self.config
        if c.get('release_config_file'):
            rc = self.query_release_config()
            self.version = rc['version']
        else:
            self.version = self._query_make_variable("MOZ_APP_VERSION")
        return self.version

    def query_upload_url(self, locale):
        if locale in self.upload_urls:
            return self.upload_urls[locale]
        else:
            self.error("Can't determine the upload url for %s!" % locale)

    def query_abs_dirs(self):
         if self.abs_dirs:
             return self.abs_dirs
         abs_dirs = super(MobileSingleLocale, self).query_abs_dirs()

         dirs = {
             'abs_tools_dir':
                 os.path.join(abs_dirs['base_work_dir'], 'tools'),
             'build_dir':
                 os.path.join(abs_dirs['base_work_dir'], 'build'),
         }

         abs_dirs.update(dirs)
         self.abs_dirs = abs_dirs
         return self.abs_dirs

    def add_failure(self, locale, message, **kwargs):
        self.locales_property[locale] = "Failed"
        prop_key = "%s_failure" % locale
        prop_value = self.query_buildbot_property(prop_key)
        if prop_value:
            prop_value = "%s  %s" % (prop_value, message)
        else:
            prop_value = message
        self.set_buildbot_property(prop_key, prop_value, write_to_file=True)
        MercurialScript.add_failure(self, locale, message=message, **kwargs)

    def summary(self):
        MercurialScript.summary(self)
        # TODO we probably want to make this configurable on/off
        locales = self.query_locales()
        for locale in locales:
            self.locales_property.setdefault(locale, "Success")
        self.set_buildbot_property("locales", json.dumps(self.locales_property), write_to_file=True)

    # Actions {{{2
    def clobber(self):
        self.read_buildbot_config()
        dirs = self.query_abs_dirs()
        c = self.config
        objdir = os.path.join(dirs['abs_work_dir'], c['mozilla_dir'],
                              c['objdir'])
        super(MobileSingleLocale, self).clobber(always_clobber_dirs=[objdir])

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
        self.pull_locale_source()

    # list_locales() is defined in LocalesMixin.

    def _setup_configure(self, buildid=None):
        c = self.config
        dirs = self.query_abs_dirs()
        env = self.query_repack_env()
        make = self.query_exe("make")
        if self.run_command_m([make, "-f", "client.mk", "configure"],
                              cwd=dirs['abs_mozilla_dir'],
                              env=env,
                              error_list=MakefileErrorList):
            self.fatal("Configure failed!")
        for make_dir in c.get('make_dirs', []):
            self.run_command_m([make, 'export'],
                               cwd=os.path.join(dirs['abs_objdir'], make_dir),
                               env=env,
                               error_list=MakefileErrorList,
                               halt_on_failure=True)
            if buildid:
                self.run_command_m([make, 'export',
                                    'MOZ_BUILD_DATE=%s' % str(buildid)],
                                   cwd=os.path.join(dirs['abs_objdir'], make_dir),
                                   env=env,
                                   error_list=MakefileErrorList)

    def setup(self):
        c = self.config
        dirs = self.query_abs_dirs()
        mozconfig_path = os.path.join(dirs['abs_mozilla_dir'], '.mozconfig')
        self.copyfile(os.path.join(dirs['abs_work_dir'], c['mozconfig']),
                      mozconfig_path)
        # TODO stop using cat
        cat = self.query_exe("cat")
        hg = self.query_exe("hg")
        make = self.query_exe("make")
        self.run_command_m([cat, mozconfig_path])
        env = self.query_repack_env()
        if self.config.get("tooltool_config"):
            self.tooltool_fetch(
                self.config['tooltool_config']['manifest'],
                bootstrap_cmd=self.config['tooltool_config'].get('bootstrap_cmd'),
                output_dir=self.config['tooltool_config']['output_dir'] % self.query_abs_dirs(),
            )
        self._setup_configure()
        self.run_command_m([make, "wget-en-US"],
                           cwd=dirs['abs_locales_dir'],
                           env=env,
                           error_list=MakefileErrorList,
                           halt_on_failure=True)
        self.run_command_m([make, "unpack"],
                           cwd=dirs['abs_locales_dir'],
                           env=env,
                           error_list=MakefileErrorList,
                           halt_on_failure=True)
        revision = self.query_revision()
        if not revision:
            self.fatal("Can't determine revision!")
        # TODO do this through VCSMixin instead of hardcoding hg
        self.run_command_m([hg, "update", "-r", revision],
                           cwd=dirs["abs_mozilla_dir"],
                           env=env,
                           error_list=BaseErrorList,
                           halt_on_failure=True)
        self.set_buildbot_property('revision', revision, write_to_file=True)
        # Configure again since the hg update may have invalidated it.
        buildid = self.query_buildid()
        self._setup_configure(buildid=buildid)

    def repack(self):
        # TODO per-locale logs and reporting.
        c = self.config
        dirs = self.query_abs_dirs()
        locales = self.query_locales()
        make = self.query_exe("make")
        repack_env = self.query_repack_env()
        base_package_name = self.query_base_package_name()
        base_package_dir = os.path.join(dirs['abs_objdir'], 'dist')
        success_count = total_count = 0
        for locale in locales:
            total_count += 1
            self.enable_mock()
            result = self.run_compare_locales(locale)
            self.disable_mock()
            if result:
                self.add_failure(locale, message="%s failed in compare-locales!" % locale)
                continue
            if self.run_command_m([make, "installers-%s" % locale],
                                  cwd=dirs['abs_locales_dir'],
                                  env=repack_env,
                                  error_list=MakefileErrorList,
                                  halt_on_failure=False):
                self.add_failure(locale, message="%s failed in make installers-%s!" % (locale, locale))
                continue
            signed_path = os.path.join(base_package_dir,
                                       base_package_name % {'locale': locale})
            # We need to wrap what this function does with mock, since
            # MobileSigningMixin doesn't know about mock
            self.enable_mock()
            status = self.verify_android_signature(
                signed_path,
                script=c['signature_verification_script'],
                env=repack_env,
                key_alias=c['key_alias'],
            )
            self.disable_mock()
            if status:
                self.add_failure(locale, message="Errors verifying %s binary!" % locale)
                # No need to rm because upload is per-locale
                continue
            success_count += 1
        self.summarize_success_count(success_count, total_count,
                                     message="Repacked %d of %d binaries successfully.")

    def upload_repacks(self):
        c = self.config
        dirs = self.query_abs_dirs()
        locales = self.query_locales()
        make = self.query_exe("make")
        base_package_name = self.query_base_package_name()
        version = self.query_version()
        upload_env = self.query_upload_env()
        success_count = total_count = 0
        buildnum = None
        if c.get('release_config_file'):
            rc = self.query_release_config()
            buildnum = rc['buildnum']
        for locale in locales:
            if self.query_failure(locale):
                self.warning("Skipping previously failed locale %s." % locale)
                continue
            total_count += 1
            if c.get('base_post_upload_cmd'):
                upload_env['POST_UPLOAD_CMD'] = c['base_post_upload_cmd'] % {'version': version, 'locale': locale, 'buildnum': str(buildnum)}
            output = self.get_output_from_command_m(
                # Ugly hack to avoid |make upload| stderr from showing up
                # as get_output_from_command errors
                "%s upload AB_CD=%s 2>&1" % (make, locale),
                cwd=dirs['abs_locales_dir'],
                env=upload_env,
                silent=True
            )
            parser = OutputParser(config=self.config, log_obj=self.log_obj,
                                  error_list=MakefileErrorList)
            parser.add_lines(output)
            if parser.num_errors:
                self.add_failure(locale, message="%s failed in make upload!" % (locale))
                continue
            package_name = base_package_name % {'locale': locale}
            r = re.compile("(http.*%s)" % package_name)
            success = False
            for line in output.splitlines():
                m = r.match(line)
                if m:
                    self.upload_urls[locale] = m.groups()[0]
                    self.info("Found upload url %s" % self.upload_urls[locale])
                    success = True
            if not success:
                self.add_failure(locale, message="Failed to detect %s url in make upload!" % (locale))
                print output
                continue
            success_count += 1
        self.summarize_success_count(success_count, total_count,
                                     message="Uploaded %d of %d binaries successfully.")

    def checkout_tools(self):
        dirs = self.query_abs_dirs()

        # We need hg.m.o/build/tools checked out
        self.info("Checking out tools")
        repos = [{
            'repo': self.config['tools_repo'],
            'vcs': "hg",  # May not have hgtool yet
            'dest': dirs['abs_tools_dir'],
        }]
        rev = self.vcs_checkout(**repos[0])
        self.set_buildbot_property("tools_revision", rev, write_to_file=True)

    def query_apkfile_path(self,locale):

        dirs = self.query_abs_dirs()
        apkdir = os.path.join(dirs['abs_objdir'], 'dist')
        r  = r"(\.)" + re.escape(locale) + r"(\.*)"

        apks = []
        for f in os.listdir(apkdir):
            if f.endswith(".apk") and re.search(r, f):
                apks.append(f)
        if len(apks) == 0:
            self.fatal("Found no apks files in %s, don't know what to do:\n%s" % (apkdir, apks), exit_code=1)

        return os.path.join(apkdir, apks[0])

    def query_is_release(self):

        return bool(self.config.get("is_release"))

    def submit_to_balrog(self):

        if not self.query_is_nightly() and not self.query_is_release():
            self.info("Not a nightly or release build, skipping balrog submission.")
            return

        if not self.config.get("balrog_servers"):
            self.info("balrog_servers not set; skipping balrog submission.")
            return

        self.checkout_tools()

        dirs = self.query_abs_dirs()
        locales = self.query_locales()
        for locale in locales:
            apkfile = self.query_apkfile_path(locale)
            apk_url = self.query_upload_url(locale)

            # Set other necessary properties for Balrog submission. None need to
            # be passed back to buildbot, so we won't write them to the properties
            #files.
            self.set_buildbot_property("locale", locale)

            self.set_buildbot_property("appVersion", self.query_version())
            # The Balrog submitter translates this platform into a build target
            # via https://github.com/mozilla/build-tools/blob/master/lib/python/release/platforms.py#L23
            self.set_buildbot_property("platform", self.buildbot_config["properties"]["platform"])
            #TODO: Is there a better way to get this?

            self.set_buildbot_property("appName", "Fennec")
            # TODO: don't hardcode
            self.set_buildbot_property("hashType", "sha512")
            self.set_buildbot_property("completeMarSize", self.query_filesize(apkfile))
            self.set_buildbot_property("completeMarHash", self.query_sha512sum(apkfile))
            self.set_buildbot_property("completeMarUrl", apk_url)
            self.set_buildbot_property("isOSUpdate", False)
            self.set_buildbot_property("buildid", self.query_buildid())

            if self.query_is_nightly():
                self.submit_balrog_updates(release_type="nightly")
            else:
                self.submit_balrog_updates(release_type="release")
        if not self.query_is_nightly():
            self.submit_balrog_release_pusher(dirs)

# main {{{1
if __name__ == '__main__':
    single_locale = MobileSingleLocale()
    single_locale.run_and_exit()
