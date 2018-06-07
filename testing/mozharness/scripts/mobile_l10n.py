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

import glob
import os
import re
import sys

try:
    import simplejson as json
    assert json
except ImportError:
    import json

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))  # noqa

from mozharness.base.errors import MakefileErrorList
from mozharness.base.log import OutputParser
from mozharness.mozilla.automation import AutomationMixin
from mozharness.mozilla.tooltool import TooltoolMixin
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.l10n.locales import LocalesMixin
from mozharness.mozilla.secrets import SecretsMixin


# MobileSingleLocale {{{1
class MobileSingleLocale(LocalesMixin, TooltoolMixin, AutomationMixin,
                         MercurialScript, SecretsMixin):
    config_options = [[
        ['--locale', ],
        {"action": "extend",
         "dest": "locales",
         "type": "string",
         "help": "Specify the locale(s) to sign and update"
         }
    ], [
        ['--tag-override', ],
        {"action": "store",
         "dest": "tag_override",
         "type": "string",
         "help": "Override the tags set for all repos"
         }
    ], [
        ['--revision', ],
        {"action": "store",
         "dest": "revision",
         "type": "string",
         "help": "Override the gecko revision to use (otherwise use automation supplied"
                 " value, or en-US revision) "}
    ], [
        ['--scm-level'],
        {"action": "store",
         "type": "int",
         "dest": "scm_level",
         "default": 1,
         "help": "This sets the SCM level for the branch being built."
                 " See https://www.mozilla.org/en-US/about/"
                 "governance/policies/commit/access-policy/"}
    ]]

    def __init__(self, require_config_file=True):
        buildscript_kwargs = {
            'all_actions': [
                "get-secrets",
                "clone-locales",
                "list-locales",
                "setup",
                "repack",
                "upload-repacks",
                "summary",
            ],
            'config': {
            },
        }
        LocalesMixin.__init__(self)
        MercurialScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            **buildscript_kwargs
        )
        self.base_package_name = None
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
        repack_env = self.query_env(partial_env=c.get("repack_env"))
        if self.query_is_nightly() or self.query_is_nightly_promotion():
            if self.query_is_nightly():
                # Nightly promotion needs to set update_channel but not do all
                # the 'IS_NIGHTLY' automation parts, like uploading symbols
                # (for now).
                repack_env["IS_NIGHTLY"] = "yes"
            # In branch_specifics.py we might set update_channel explicitly.
            if c.get('update_channel'):
                repack_env["MOZ_UPDATE_CHANNEL"] = c['update_channel']
            else:  # Let's just give the generic channel based on branch.
                repack_env["MOZ_UPDATE_CHANNEL"] = \
                    "nightly-%s" % (c['branch'],)

        self.repack_env = repack_env
        return self.repack_env

    def query_l10n_env(self):
        return self.query_env()

    def query_upload_env(self):
        if self.upload_env:
            return self.upload_env

        upload_env = self.query_env(partial_env=self.config.get("upload_env"),
                                    replace_dict=dict(self.config))
        self.upload_env = upload_env
        return self.upload_env

    def query_revision(self):
        """ Get the gecko revision in this order of precedence
              * cached value
              * command line arg --revision   (development, taskcluster)
              * from the en-US build          (m-c & m-a)

        This will fail the last case if the build hasn't been pulled yet.
        """
        if self.revision:
            return self.revision

        config = self.config
        revision = None
        if config.get("revision"):
            revision = config["revision"]
        if not revision:
            self.fatal("Can't determine revision!")
        self.revision = str(revision)
        return self.revision

    def _query_make_variable(self, variable, make_args=None):
        make = self.query_exe('make')
        env = self.query_repack_env()
        dirs = self.query_abs_dirs()
        if make_args is None:
            make_args = []
        # TODO error checking
        output = self.get_output_from_command(
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
             'abs_tools_dir': os.path.join(abs_dirs['base_work_dir'], 'tools'),
             'build_dir': os.path.join(abs_dirs['base_work_dir'], 'build'),
        }

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def add_failure(self, locale, message, **kwargs):
        self.locales_property[locale] = "Failed"
        prop_key = "%s_failure" % locale
        prop_value = self.query_property(prop_key)
        if prop_value:
            prop_value = "%s  %s" % (prop_value, message)
        else:
            prop_value = message
        self.set_property(prop_key, prop_value)
        MercurialScript.add_failure(self, locale, message=message, **kwargs)

    def summary(self):
        MercurialScript.summary(self)
        # TODO we probably want to make this configurable on/off
        locales = self.query_locales()
        for locale in locales:
            self.locales_property.setdefault(locale, "Success")
        self.set_property("locales", json.dumps(self.locales_property))

    # Actions {{{2
    def clone_locales(self):
        self.pull_locale_source()

    # list_locales() is defined in LocalesMixin.

    def _setup_configure(self):
        dirs = self.query_abs_dirs()
        env = self.query_repack_env()

        mach = os.path.join(dirs['abs_mozilla_dir'], 'mach')

        if self.run_command([sys.executable, mach, 'configure'],
                            cwd=dirs['abs_mozilla_dir'],
                            env=env,
                            error_list=MakefileErrorList):
            self.fatal("Configure failed!")

        # Invoke the build system to get nsinstall and buildid.h.
        targets = [
            'config/export',
            'buildid.h',
        ]

        self.run_command([sys.executable, mach, 'build'] + targets,
                         cwd=dirs['abs_mozilla_dir'],
                         env=env,
                         error_list=MakefileErrorList,
                         halt_on_failure=True)

    def setup(self):
        c = self.config
        dirs = self.query_abs_dirs()
        mozconfig_path = os.path.join(dirs['abs_mozilla_dir'], '.mozconfig')
        self.copyfile(os.path.join(dirs['abs_work_dir'], c['mozconfig']),
                      mozconfig_path)
        # TODO stop using cat
        cat = self.query_exe("cat")
        make = self.query_exe("make")
        self.run_command([cat, mozconfig_path])
        env = self.query_repack_env()
        if self.config.get("tooltool_config"):
            self.tooltool_fetch(
                self.config['tooltool_config']['manifest'],
                output_dir=self.config['tooltool_config']['output_dir'] % self.query_abs_dirs(),
            )
        self._setup_configure()
        self.run_command([make, "wget-en-US"],
                         cwd=dirs['abs_locales_dir'],
                         env=env,
                         error_list=MakefileErrorList,
                         halt_on_failure=True)
        self.run_command([make, "unpack"],
                         cwd=dirs['abs_locales_dir'],
                         env=env,
                         error_list=MakefileErrorList,
                         halt_on_failure=True)

    def repack(self):
        # TODO per-locale logs and reporting.
        dirs = self.query_abs_dirs()
        locales = self.query_locales()
        make = self.query_exe("make")
        repack_env = self.query_repack_env()
        success_count = total_count = 0
        for locale in locales:
            total_count += 1
            if self.run_command([make, "installers-%s" % locale],
                                cwd=dirs['abs_locales_dir'],
                                env=repack_env,
                                error_list=MakefileErrorList,
                                halt_on_failure=False):
                self.add_failure(locale, message="%s failed in make installers-%s!" %
                                 (locale, locale))
                continue
            success_count += 1
        self.summarize_success_count(success_count, total_count,
                                     message="Repacked %d of %d binaries successfully.")

    def upload_repacks(self):
        dirs = self.query_abs_dirs()
        locales = self.query_locales()
        make = self.query_exe("make")
        base_package_name = self.query_base_package_name()
        upload_env = self.query_upload_env()
        success_count = total_count = 0
        for locale in locales:
            if self.query_failure(locale):
                self.warning("Skipping previously failed locale %s." % locale)
                continue
            total_count += 1
            output = self.get_output_from_command(
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
            for line in output.splitlines():
                m = r.match(line)
                if m:
                    self.upload_urls[locale] = m.groups()[0]
                    self.info("Found upload url %s" % self.upload_urls[locale])

            # XXX Move the files to a SIMPLE_NAME format until we can enable
            #     Simple names in the build system
            if self.config.get("simple_name_move"):
                # Assume an UPLOAD PATH
                upload_target = self.config["upload_env"]["UPLOAD_PATH"]
                target_path = os.path.join(upload_target, locale)
                self.mkdir_p(target_path)
                glob_name = "*.%s.android-arm.*" % locale
                for f in glob.glob(os.path.join(upload_target, glob_name)):
                    glob_extension = f[f.rfind('.'):]
                    self.move(os.path.join(f),
                              os.path.join(target_path, "target%s" % glob_extension))
                self.log("Converted uploads for %s to simple names" % locale)
            success_count += 1
        self.summarize_success_count(success_count, total_count,
                                     message="Make Upload for %d of %d locales successful.")


# main {{{1
if __name__ == '__main__':
    single_locale = MobileSingleLocale()
    single_locale.run_and_exit()
