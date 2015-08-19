#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""desktop_l10n.py

This script manages Desktop repacks for nightly builds.
"""
import os
import re
import sys
import shlex
import logging

import subprocess

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import BaseErrorList, MakefileErrorList
from mozharness.base.script import BaseScript
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.buildbot import BuildbotMixin
from mozharness.mozilla.purge import PurgeMixin
from mozharness.mozilla.building.buildbase import MakeUploadOutputParser
from mozharness.mozilla.l10n.locales import LocalesMixin
from mozharness.mozilla.mar import MarMixin
from mozharness.mozilla.mock import MockMixin
from mozharness.mozilla.release import ReleaseMixin
from mozharness.mozilla.signing import SigningMixin
from mozharness.mozilla.updates.balrog import BalrogMixin
from mozharness.mozilla.taskcluster_helper import Taskcluster
from mozharness.base.python import VirtualenvMixin
from mozharness.mozilla.mock import ERROR_MSGS
from mozharness.base.log import FATAL

try:
    import simplejson as json
    assert json
except ImportError:
    import json


# needed by _map
SUCCESS = 0
FAILURE = 1

# when running get_output_form_command, pymake has some extra output
# that needs to be filtered out
PyMakeIgnoreList = [
    re.compile(r'''.*make\.py(?:\[\d+\])?: Entering directory'''),
    re.compile(r'''.*make\.py(?:\[\d+\])?: Leaving directory'''),
]


# mandatory configuration options, without them, this script will not work
# it's a list of values that are already known before starting a build
configuration_tokens = ('branch',
                        'platform',
                        'en_us_binary_url',
                        'update_platform',
                        'update_channel',
                        'ssh_key_dir',
                        'stage_product',
                        )
# some other values such as "%(version)s", "%(buildid)s", ...
# are defined at run time and they cannot be enforced in the _pre_config_lock
# phase
runtime_config_tokens = ('buildid', 'version', 'locale', 'from_buildid',
                         'abs_objdir', 'abs_merge_dir', 'version', 'to_buildid')


# DesktopSingleLocale {{{1
class DesktopSingleLocale(LocalesMixin, ReleaseMixin, MockMixin, BuildbotMixin,
                          VCSMixin, SigningMixin, PurgeMixin, BaseScript,
                          BalrogMixin, MarMixin, VirtualenvMixin, TransferMixin):
    """Manages desktop repacks"""
    config_options = [[
        ['--balrog-config', ],
        {"action": "extend",
         "dest": "config_files",
         "type": "string",
         "help": "Specify the balrog configuration file"}
    ], [
        ['--branch-config', ],
        {"action": "extend",
         "dest": "config_files",
         "type": "string",
         "help": "Specify the branch configuration file"}
    ], [
        ['--environment-config', ],
        {"action": "extend",
         "dest": "config_files",
         "type": "string",
         "help": "Specify the environment (staging, production, ...) configuration file"}
    ], [
        ['--platform-config', ],
        {"action": "extend",
         "dest": "config_files",
         "type": "string",
         "help": "Specify the platform configuration file"}
    ], [
        ['--locale', ],
        {"action": "extend",
         "dest": "locales",
         "type": "string",
         "help": "Specify the locale(s) to sign and update"}
    ], [
        ['--locales-file', ],
        {"action": "store",
         "dest": "locales_file",
         "type": "string",
         "help": "Specify a file to determine which locales to sign and update"}
    ], [
        ['--tag-override', ],
        {"action": "store",
         "dest": "tag_override",
         "type": "string",
         "help": "Override the tags set for all repos"}
    ], [
        ['--user-repo-override', ],
        {"action": "store",
         "dest": "user_repo_override",
         "type": "string",
         "help": "Override the user repo path for all repos"}
    ], [
        ['--release-config-file', ],
        {"action": "store",
         "dest": "release_config_file",
         "type": "string",
         "help": "Specify the release config file to use"}
    ], [
        ['--this-chunk', ],
        {"action": "store",
         "dest": "this_locale_chunk",
         "type": "int",
         "help": "Specify which chunk of locales to run"}
    ], [
        ['--total-chunks', ],
        {"action": "store",
         "dest": "total_locale_chunks",
         "type": "int",
         "help": "Specify the total number of chunks of locales"}
    ], [
        ['--en-us-installer-url', ],
        {"action": "store",
         "dest": "en_us_installer_url",
         "type": "string",
         "help": "Specify the url of the en-us binary"}
    ]]

    def __init__(self, require_config_file=True):
        # fxbuild style:
        buildscript_kwargs = {
            'all_actions': [
                "clobber",
                "pull",
                "list-locales",
                "setup",
                "repack",
                "create-virtualenv",
                "taskcluster-upload",
                "funsize-props",
                "submit-to-balrog",
                "summary",
            ],
            'config': {
                "buildbot_json_path": "buildprops.json",
                "ignore_locales": ["en-US"],
                "locales_dir": "browser/locales",
                "update_mar_dir": "dist/update",
                "buildid_section": "App",
                "buildid_option": "BuildID",
                "application_ini": "application.ini",
                "log_name": "single_locale",
                "clobber_file": 'CLOBBER',
                "appName": "Firefox",
                "hashType": "sha512",
                "taskcluster_credentials_file": "oauth.txt",
                'virtualenv_modules': [
                    'requests==2.2.1',
                    'PyHawk-with-a-single-extra-commit==0.1.5',
                    'taskcluster==0.0.15',
                ],
                'virtualenv_path': 'venv',
            },
        }
        #

        LocalesMixin.__init__(self)
        BaseScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            **buildscript_kwargs
        )

        self.buildid = None
        self.make_ident_output = None
        self.bootstrap_env = None
        self.upload_env = None
        self.revision = None
        self.version = None
        self.upload_urls = {}
        self.locales_property = {}
        self.l10n_dir = None
        self.package_urls = {}
        self.pushdate = None
        # upload_files is a dictionary of files to upload, keyed by locale.
        self.upload_files = {}

        if 'mock_target' in self.config:
            self.enable_mock()

    def _pre_config_lock(self, rw_config):
        """replaces 'configuration_tokens' with their values, before the
           configuration gets locked. If some of the configuration_tokens
           are not present, stops the execution of the script"""
        # since values as branch, platform are mandatory, can replace them in
        # in the configuration before it is locked down
        # mandatory tokens
        for token in configuration_tokens:
            if token not in self.config:
                self.fatal('No %s in configuration!' % token)

        # all the important tokens are present in our configuration
        for token in configuration_tokens:
            # token_string '%(branch)s'
            token_string = ''.join(('%(', token, ')s'))
            # token_value => ash
            token_value = self.config[token]
            for element in self.config:
                # old_value =>  https://hg.mozilla.org/projects/%(branch)s
                old_value = self.config[element]
                # new_value => https://hg.mozilla.org/projects/ash
                new_value = self.__detokenise_element(self.config[element],
                                                      token_string,
                                                      token_value)
                if new_value and new_value != old_value:
                    msg = "%s: replacing %s with %s" % (element,
                                                        old_value,
                                                        new_value)
                    self.debug(msg)
                    self.config[element] = new_value

        # now, only runtime_config_tokens should be present in config
        # we should parse self.config and fail if any other  we spot any
        # other token
        tokens_left = set(self._get_configuration_tokens(self.config))
        unknown_tokens = set(tokens_left) - set(runtime_config_tokens)
        if unknown_tokens:
            msg = ['unknown tokens in configuration:']
            for t in unknown_tokens:
                msg.append(t)
            self.fatal(' '.join(msg))
        self.info('configuration looks ok')

    def _get_configuration_tokens(self, iterable):
        """gets a list of tokens in iterable"""
        regex = re.compile('%\(\w+\)s')
        results = []
        try:
            for element in iterable:
                if isinstance(iterable, str):
                    # this is a string, look for tokens
                    # self.debug("{0}".format(re.findall(regex, element)))
                    tokens = re.findall(regex, iterable)
                    for token in tokens:
                        # clean %(branch)s => branch
                        # remove %(
                        token_name = token.partition('%(')[2]
                        # remove )s
                        token_name = token_name.partition(')s')[0]
                        results.append(token_name)
                    break

                elif isinstance(iterable, (list, tuple)):
                    results.extend(self._get_configuration_tokens(element))

                elif isinstance(iterable, dict):
                    results.extend(self._get_configuration_tokens(iterable[element]))

        except TypeError:
            # element is a int/float/..., nothing to do here
            pass

        # remove duplicates, and return results

        return list(set(results))

    def __detokenise_element(self, config_option, token, value):
        """reads config_options and returns a version of the same config_option
           replacing token with value recursively"""
        # config_option is a string, let's replace token with value
        if isinstance(config_option, str):
            # if token does not appear in this string,
            # nothing happens and the original value is returned
            return config_option.replace(token, value)
        # it's a dictionary
        elif isinstance(config_option, dict):
            # replace token for each element of this dictionary
            for element in config_option:
                config_option[element] = self.__detokenise_element(
                    config_option[element], token, value)
            return config_option
        # it's a list
        elif isinstance(config_option, list):
            # create a new list and append the replaced elements
            new_list = []
            for element in config_option:
                new_list.append(self.__detokenise_element(element, token, value))
            return new_list
        elif isinstance(config_option, tuple):
            # create a new list and append the replaced elements
            new_list = []
            for element in config_option:
                new_list.append(self.__detokenise_element(element, token, value))
            return tuple(new_list)
        # everything else, bool, number, ...
        return None

    # Helper methods {{{2
    def query_bootstrap_env(self):
        """returns the env for repacks"""
        if self.bootstrap_env:
            return self.bootstrap_env
        config = self.config
        replace_dict = self.query_abs_dirs()
        bootstrap_env = self.query_env(partial_env=config.get("bootstrap_env"),
                                       replace_dict=replace_dict)
        if config.get('en_us_binary_url') and \
           config.get('release_config_file'):
            bootstrap_env['EN_US_BINARY_URL'] = config['en_us_binary_url']
        if 'MOZ_SIGNING_SERVERS' in os.environ:
            sign_cmd = self.query_moz_sign_cmd(formats=None)
            sign_cmd = subprocess.list2cmdline(sign_cmd)
            # windows fix
            bootstrap_env['MOZ_SIGN_CMD'] = sign_cmd.replace('\\', '\\\\\\\\')
        for binary in self._mar_binaries():
            # "mar -> MAR" and 'mar.exe -> MAR' (windows)
            name = binary.replace('.exe', '')
            name = name.upper()
            binary_path = os.path.join(self._mar_tool_dir(), binary)
            # windows fix...
            if binary.endswith('.exe'):
                binary_path = binary_path.replace('\\', '\\\\\\\\')
            bootstrap_env[name] = binary_path
        if self.query_is_nightly():
            bootstrap_env["IS_NIGHTLY"] = "yes"
        self.bootstrap_env = bootstrap_env
        return self.bootstrap_env

    def _query_upload_env(self):
        """returns the environment used for the upload step"""
        if self.upload_env:
            return self.upload_env
        config = self.config
        buildid = self._query_buildid()
        version = self.query_version()
        upload_env = self.query_env(partial_env=config.get("upload_env"),
                                    replace_dict={'buildid': buildid,
                                                  'version': version})
        upload_env['LATEST_MAR_DIR'] = config['latest_mar_dir']
        # check if there are any extra option from the platform configuration
        # and append them to the env

        if 'upload_env_extra' in config:
            for extra in config['upload_env_extra']:
                upload_env[extra] = config['upload_env_extra'][extra]
        self.upload_env = upload_env
        return self.upload_env

    def query_l10n_env(self):
        l10n_env = self._query_upload_env().copy()
        # both upload_env and bootstrap_env define MOZ_SIGN_CMD
        # the one from upload_env is taken from os.environ, the one from
        # bootstrap_env is set with query_moz_sign_cmd()
        # we need to use the value provided my query_moz_sign_cmd or make upload
        # will fail (signtool.py path is wrong)
        l10n_env.update(self.query_bootstrap_env())
        return l10n_env

    def _query_make_ident_output(self):
        """Get |make ident| output from the objdir.
        Only valid after setup is run.
       """
        if self.make_ident_output:
            return self.make_ident_output
        dirs = self.query_abs_dirs()
        self.make_ident_output = self._get_output_from_make(
            target=["ident"],
            cwd=dirs['abs_locales_dir'],
            env=self.query_bootstrap_env())
        return self.make_ident_output

    def _query_buildid(self):
        """Get buildid from the objdir.
        Only valid after setup is run.
       """
        if self.buildid:
            return self.buildid
        r = re.compile(r"buildid (\d+)")
        output = self._query_make_ident_output()
        for line in output.splitlines():
            match = r.match(line)
            if match:
                self.buildid = match.groups()[0]
        return self.buildid

    def _query_revision(self):
        """Get revision from the objdir.
        Only valid after setup is run.
       """
        if self.revision:
            return self.revision
        r = re.compile(r"^(gecko|fx)_revision ([0-9a-f]+\+?)$")
        output = self._query_make_ident_output()
        for line in output.splitlines():
            match = r.match(line)
            if match:
                self.revision = match.groups()[1]
        return self.revision

    def _query_make_variable(self, variable, make_args=None,
                             exclude_lines=PyMakeIgnoreList):
        """returns the value of make echo-variable-<variable>
           it accepts extra make arguements (make_args)
           it also has an exclude_lines from the output filer
           exclude_lines defaults to PyMakeIgnoreList because
           on windows, pymake writes extra output lines that need
           to be filtered out.
        """
        dirs = self.query_abs_dirs()
        make_args = make_args or []
        exclude_lines = exclude_lines or []
        target = ["echo-variable-%s" % variable] + make_args
        cwd = dirs['abs_locales_dir']
        raw_output = self._get_output_from_make(target, cwd=cwd,
                                                env=self.query_bootstrap_env())
        # we want to log all the messages from make/pymake and
        # exlcude some messages from the output ("Entering directory...")
        output = []
        for line in raw_output.split("\n"):
            discard = False
            for element in exclude_lines:
                if element.match(line):
                    discard = True
                    continue
            if not discard:
                output.append(line.strip())
        output = " ".join(output).strip()
        self.info('echo-variable-%s: %s' % (variable, output))
        return output

    def query_version(self):
        """Gets the version from the objdir.
        Only valid after setup is run."""
        if self.version:
            return self.version
        config = self.config
        if config.get('release_config_file'):
            release_config = self.query_release_config()
            self.version = release_config['version']
        else:
            self.version = self._query_make_variable("MOZ_APP_VERSION")
        return self.version

    def _map(self, func, items):
        """runs func for any item in items, calls the add_failure() for each
           error. It assumes that function returns 0 when successful.
           returns a two element tuple with (success_count, total_count)"""
        success_count = 0
        total_count = len(items)
        name = func.__name__
        for item in items:
            result = func(item)
            if result == SUCCESS:
                #  success!
                success_count += 1
            else:
                #  func failed...
                message = 'failure: %s(%s)' % (name, item)
                self._add_failure(item, message)
        return (success_count, total_count)

    def _add_failure(self, locale, message, **kwargs):
        """marks current step as failed"""
        self.locales_property[locale] = "Failed"
        prop_key = "%s_failure" % locale
        prop_value = self.query_buildbot_property(prop_key)
        if prop_value:
            prop_value = "%s  %s" % (prop_value, message)
        else:
            prop_value = message
        self.set_buildbot_property(prop_key, prop_value, write_to_file=True)
        BaseScript.add_failure(self, locale, message=message, **kwargs)

    def summary(self):
        """generates a summary"""
        BaseScript.summary(self)
        # TODO we probably want to make this configurable on/off
        locales = self.query_locales()
        for locale in locales:
            self.locales_property.setdefault(locale, "Success")
        self.set_buildbot_property("locales",
                                   json.dumps(self.locales_property),
                                   write_to_file=True)

    # Actions {{{2
    def clobber(self):
        """clobber"""
        dirs = self.query_abs_dirs()
        clobber_dirs = (dirs['abs_objdir'], dirs['abs_compare_locales_dir'],
                        dirs['abs_upload_dir'])
        PurgeMixin.clobber(self, always_clobber_dirs=clobber_dirs)

    def pull(self):
        """pulls source code"""
        config = self.config
        dirs = self.query_abs_dirs()
        repos = []
        # replace dictionary for repos
        # we need to interpolate some values:
        # branch, branch_repo
        # and user_repo_override if exists
        replace_dict = {}
        if config.get("user_repo_override"):
            replace_dict['user_repo_override'] = config['user_repo_override']

        for repository in config['repos']:
            current_repo = {}
            for key, value in repository.iteritems():
                try:
                    current_repo[key] = value % replace_dict
                except KeyError:
                    self.error('not all the values in "{0}" can be replaced. Check your configuration'.format(value))
                    raise
            repos.append(current_repo)
        self.info("repositories: %s" % repos)
        self.vcs_checkout_repos(repos, parent_dir=dirs['abs_work_dir'],
                                tag_override=config.get('tag_override'))
        self.pull_locale_source()

    def setup(self):
        """setup step"""
        dirs = self.query_abs_dirs()
        self._run_tooltool()
        self._copy_mozconfig()
        self._mach_configure()
        self._run_make_in_config_dir()
        self.make_wget_en_US()
        self.make_unpack_en_US()
        revision = self._query_revision()
        if not revision:
            self.fatal("Can't determine revision!")
        #  TODO do this through VCSMixin instead of hardcoding hg
        #  self.update(dest=dirs["abs_mozilla_dir"], revision=revision)
        hg = self.query_exe("hg")
        self.run_command([hg, "update", "-r", revision],
                         cwd=dirs["abs_mozilla_dir"],
                         env=self.query_bootstrap_env(),
                         error_list=BaseErrorList,
                         halt_on_failure=True, fatal_exit_code=3)
        # if checkout updates CLOBBER file with a newer timestamp,
        # next make -f client.mk configure  will delete archives
        # downloaded with make wget_en_US, so just touch CLOBBER file
        _clobber_file = self._clobber_file()
        if os.path.exists(_clobber_file):
            self._touch_file(_clobber_file)
        # and again...
        # thanks to the last hg update, we can be on different firefox 'version'
        # than the one on default,
        self._mach_configure()
        self._run_make_in_config_dir()
        self.download_mar_tools()

    def _run_make_in_config_dir(self):
        """this step creates nsinstall, needed my make_wget_en_US()
        """
        dirs = self.query_abs_dirs()
        config_dir = os.path.join(dirs['abs_objdir'], 'config')
        env = self.query_bootstrap_env()
        return self._make(target=['export'], cwd=config_dir, env=env)

    def _clobber_file(self):
        """returns the full path of the clobber file"""
        config = self.config
        dirs = self.query_abs_dirs()
        return os.path.join(dirs['abs_objdir'], config.get('clobber_file'))

    def _copy_mozconfig(self):
        """copies the mozconfig file into abs_mozilla_dir/.mozconfig
           and logs the content
        """
        config = self.config
        dirs = self.query_abs_dirs()
        mozconfig = config['mozconfig']
        src = os.path.join(dirs['abs_work_dir'], mozconfig)
        dst = os.path.join(dirs['abs_mozilla_dir'], '.mozconfig')
        self.copyfile(src, dst)

        # STUPID HACK HERE
        # should we update the mozconfig so it has the right value?
        with self.opened(src, 'r') as (in_mozconfig, in_error):
            if in_error:
                self.fatal('cannot open {0}'.format(src))
            with self.opened(dst, open_mode='w') as (out_mozconfig, out_error):
                if out_error:
                    self.fatal('cannot write {0}'.format(dst))
                for line in in_mozconfig:
                    if 'with-l10n-base' in line:
                        line = 'ac_add_options --with-l10n-base=../../l10n\n'
                        self.l10n_dir = line.partition('=')[2].strip()
                    out_mozconfig.write(line)
        # now log
        with self.opened(dst, 'r') as (mozconfig, in_error):
            if in_error:
                self.fatal('cannot open {0}'.format(dst))
            for line in mozconfig:
                self.info(line.strip())

    def _mach(self, target, env, halt_on_failure=True, output_parser=None):
        dirs = self.query_abs_dirs()
        mach = self._get_mach_executable()
        return self.run_command(mach + target,
                                halt_on_failure=True,
                                env=env,
                                cwd=dirs['abs_mozilla_dir'],
                                output_parser=None)

    def _mach_configure(self):
        """calls mach configure"""
        env = self.query_bootstrap_env()
        target = ["configure"]
        return self._mach(target=target, env=env)

    def _get_mach_executable(self):
        python = self.query_exe('python2.7')
        return [python, 'mach']

    def _get_make_executable(self):
        config = self.config
        dirs = self.query_abs_dirs()
        if config.get('enable_mozmake'):  # e.g. windows
            make = r"/".join([dirs['abs_mozilla_dir'], 'mozmake.exe'])
            # mysterious subprocess errors, let's try to fix this path...
            make = make.replace('\\', '/')
            make = [make]
        else:
            make = ['make']
        return make

    def _make(self, target, cwd, env, error_list=MakefileErrorList,
              halt_on_failure=True, output_parser=None):
        """Runs make. Returns the exit code"""
        make = self._get_make_executable()
        if target:
            make = make + target
        return self.run_command(make,
                                cwd=cwd,
                                env=env,
                                error_list=error_list,
                                halt_on_failure=halt_on_failure,
                                output_parser=output_parser)

    def _get_output_from_make(self, target, cwd, env, halt_on_failure=True):
        """runs make and returns the output of the command"""
        make = self._get_make_executable()
        return self.get_output_from_command(make + target,
                                            cwd=cwd,
                                            env=env,
                                            silent=True,
                                            halt_on_failure=halt_on_failure)

    def make_unpack_en_US(self):
        """wrapper for make unpack"""
        config = self.config
        dirs = self.query_abs_dirs()
        env = self.query_bootstrap_env()
        cwd = os.path.join(dirs['abs_objdir'], config['locales_dir'])
        return self._make(target=["unpack"], cwd=cwd, env=env)

    def make_wget_en_US(self):
        """wrapper for make wget-en-US"""
        env = self.query_bootstrap_env()
        dirs = self.query_abs_dirs()
        cwd = dirs['abs_locales_dir']
        binary_file = env['EN_US_BINARY_URL']
        if binary_file.endswith(('tar.bz2', 'dmg', 'exe')):
            # specified EN_US_BINARY url is an installer file...
            dst_filename = binary_file.split('/')[-1].strip()
            dst_filename = os.path.join(dirs['abs_objdir'], 'dist', dst_filename)
            # we need to set ZIP_IN so make unpack finds this binary file.
            # Please note this is required only if the en-us-binary-url provided
            # has a different version number from the one in the current
            # checkout.
            self.bootstrap_env['ZIP_IN'] = dst_filename
            return self._retry_download_file(binary_file, dst_filename, error_level=FATAL)

        # binary url is not an installer, use make wget-en-US to download it
        return self._make(target=["wget-en-US"], cwd=cwd, env=env)

    def make_upload(self, locale):
        """wrapper for make upload command"""
        config = self.config
        env = self.query_l10n_env()
        dirs = self.query_abs_dirs()
        buildid = self._query_buildid()
        try:
            env['POST_UPLOAD_CMD'] = config['base_post_upload_cmd'] % {'buildid': buildid,
                                                                       'branch': config['branch']}
        except KeyError:
            # no base_post_upload_cmd in configuration, just skip it
            pass
        target = ['upload', 'AB_CD=%s' % (locale)]
        cwd = dirs['abs_locales_dir']
        parser = MakeUploadOutputParser(config=self.config,
                                        log_obj=self.log_obj)
        retval = self._make(target=target, cwd=cwd, env=env,
                            halt_on_failure=False, output_parser=parser)
        if locale not in self.package_urls:
            self.package_urls[locale] = {}
        self.package_urls[locale].update(parser.matches)
        if 'partialMarUrl' in self.package_urls[locale]:
            self.package_urls[locale]['partialInfo'] = self._get_partial_info(
                self.package_urls[locale]['partialMarUrl'])
        if retval == SUCCESS:
            self.info('Upload successful (%s)' % (locale))
            ret = SUCCESS
        else:
            self.error('failed to upload %s' % (locale))
            ret = FAILURE
        return ret

    def get_upload_files(self, locale):
        # The tree doesn't have a good way of exporting the list of files
        # created during locale generation, but we can grab them by echoing the
        # UPLOAD_FILES variable for each locale.
        env = self.query_l10n_env()
        target = ['echo-variable-UPLOAD_FILES', 'AB_CD=%s' % (locale)]
        dirs = self.query_abs_dirs()
        cwd = dirs['abs_locales_dir']
        output = self._get_output_from_make(target=target, cwd=cwd, env=env)
        self.info('UPLOAD_FILES is "%s"' % (output))
        files = shlex.split(output)
        if not files:
            self.error('failed to get upload file list for locale %s' % (locale))
            return FAILURE

        self.upload_files[locale] = [
            os.path.abspath(os.path.join(cwd, f)) for f in files
        ]
        return SUCCESS

    def make_installers(self, locale):
        """wrapper for make installers-(locale)"""
        env = self.query_l10n_env()
        self._copy_mozconfig()
        env['L10NBASEDIR'] = self.l10n_dir
        dirs = self.query_abs_dirs()
        cwd = os.path.join(dirs['abs_locales_dir'])
        target = ["installers-%s" % locale,
                  "LOCALE_MERGEDIR=%s" % env["LOCALE_MERGEDIR"], ]
        return self._make(target=target, cwd=cwd,
                          env=env, halt_on_failure=False)

    def repack_locale(self, locale):
        """wraps the logic for comapare locale, make installers and generate
           complete updates."""

        if self.run_compare_locales(locale) != SUCCESS:
            self.error("compare locale %s failed" % (locale))
            return FAILURE

        # compare locale succeeded, run make installers
        if self.make_installers(locale) != SUCCESS:
            self.error("make installers-%s failed" % (locale))
            return FAILURE

        if self.get_upload_files(locale):
            self.error("failed to get list of files to upload for locale %s" % (locale))
            return FAILURE
        # now try to upload the artifacts
        if self.make_upload(locale):
            self.error("make upload for locale %s failed!" % (locale))
            return FAILURE
        return SUCCESS

    def repack(self):
        """creates the repacks and udpates"""
        self._map(self.repack_locale, self.query_locales())

    def _query_objdir(self):
        """returns objdir name from configuration"""
        return self.config['objdir']

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(DesktopSingleLocale, self).query_abs_dirs()
        for directory in abs_dirs:
            value = abs_dirs[directory]
            abs_dirs[directory] = value
        dirs = {}
        dirs['abs_tools_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'tools')
        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def _get_partial_info(self, partial_url):
        """takes a partial url and returns a partial info dictionary"""
        partial_file = partial_url.split('/')[-1]
        # now get from_build...
        # firefox-39.0a1.ar.win32.partial.20150320030211-20150320075143.mar
        # partial file ^                  ^            ^
        #                                 |            |
        # we need ------------------------+------------+
        from_buildid = partial_file.partition('partial.')[2]
        from_buildid = from_buildid.partition('-')[0]
        self.info('from buildid: {0}'.format(from_buildid))

        dirs = self.query_abs_dirs()
        abs_partial_file = os.path.join(dirs['abs_objdir'], 'dist',
                                        'update', partial_file)

        size = self.query_filesize(abs_partial_file)
        hash_ = self.query_sha512sum(abs_partial_file)
        return [{'from_buildid': from_buildid,
                 'hash': hash_,
                 'size': size,
                 'url': partial_url}]

    def submit_to_balrog(self):
        """submit to barlog"""
        if not self.config.get("balrog_servers"):
            self.info("balrog_servers not set; skipping balrog submission.")
            return
        self.info("Reading buildbot build properties...")
        self.read_buildbot_config()
        # get platform, appName and hashType from configuration
        # common values across different locales
        config = self.config
        platform = config["platform"]
        hashType = config['hashType']
        appName = config['appName']
        branch = config['branch']
        # values from configuration
        self.set_buildbot_property("branch", branch)
        self.set_buildbot_property("appName", appName)
        # it's hardcoded to sha512 in balrog.py
        self.set_buildbot_property("hashType", hashType)
        self.set_buildbot_property("platform", platform)
        # values common to the current repacks
        self.set_buildbot_property("buildid", self._query_buildid())
        self.set_buildbot_property("appVersion", self.query_version())

        # submit complete mar to balrog
        # clean up buildbot_properties
        self._map(self.submit_repack_to_balrog, self.query_locales())

    def submit_repack_to_balrog(self, locale):
        """submit a single locale to balrog"""
        # check if locale has been uploaded, if not just return a FAILURE
        if locale not in self.package_urls:
            self.error("%s is not present in package_urls. Did you run make upload?" % locale)
            return FAILURE

        if not self.query_is_nightly():
            # remove this check when we extend this script to non-nightly builds
            self.fatal("Not a nightly build")
            return FAILURE

        # complete mar file
        c_marfile = self._query_complete_mar_filename(locale)
        c_mar_url = self._query_complete_mar_url(locale)

        # Set other necessary properties for Balrog submission. None need to
        # be passed back to buildbot, so we won't write them to the properties
        # files
        # Locale is hardcoded to en-US, for silly reasons
        # The Balrog submitter translates this platform into a build target
        # via https://github.com/mozilla/build-tools/blob/master/lib/python/release/platforms.py#L23
        self.set_buildbot_property("completeMarSize", self.query_filesize(c_marfile))
        self.set_buildbot_property("completeMarHash", self.query_sha512sum(c_marfile))
        self.set_buildbot_property("completeMarUrl", c_mar_url)
        self.set_buildbot_property("locale", locale)
        if "partialInfo" in self.package_urls[locale]:
            self.set_buildbot_property("partialInfo",
                                       self.package_urls[locale]["partialInfo"])
        ret = FAILURE
        try:
            result = self.submit_balrog_updates()
            self.info("balrog return code: %s" % (result))
            if result == 0:
                ret = SUCCESS
        except Exception as error:
            self.error("submit repack to balrog failed: %s" % (str(error)))
        return ret

    def _query_complete_mar_filename(self, locale):
        """returns the full path to a localized complete mar file"""
        config = self.config
        version = self.query_version()
        complete_mar_name = config['localized_mar'] % {'version': version,
                                                       'locale': locale}
        return os.path.join(self._update_mar_dir(), complete_mar_name)

    def _query_complete_mar_url(self, locale):
        """returns the complete mar url taken from self.package_urls[locale]
           this value is available only after make_upload"""
        if "complete_mar_url" in self.config:
            return self.config["complete_mar_url"]
        if "completeMarUrl" in self.package_urls[locale]:
            return self.package_urls[locale]["completeMarUrl"]
        # url = self.config.get("update", {}).get("mar_base_url")
        # if url:
        #    url += os.path.basename(self.query_marfile_path())
        #    return url.format(branch=self.query_branch())
        self.fatal("Couldn't find complete mar url in config or package_urls")

    def _update_mar_dir(self):
        """returns the full path of the update/ directory"""
        return self._mar_dir('update_mar_dir')

    def _mar_binaries(self):
        """returns a tuple with mar and mbsdiff paths"""
        config = self.config
        return (config['mar'], config['mbsdiff'])

    def _mar_dir(self, dirname):
        """returns the full path of dirname;
            dirname is an entry in configuration"""
        dirs = self.query_abs_dirs()
        return os.path.join(dirs['abs_objdir'], self.config[dirname])

    # TODO: replace with ToolToolMixin
    def _get_tooltool_auth_file(self):
        # set the default authentication file based on platform; this
        # corresponds to where puppet puts the token
        if 'tooltool_authentication_file' in self.config:
            fn = self.config['tooltool_authentication_file']
        elif self._is_windows():
            fn = r'c:\builds\relengapi.tok'
        else:
            fn = '/builds/relengapi.tok'

        # if the file doesn't exist, don't pass it to tooltool (it will just
        # fail).  In taskcluster, this will work OK as the relengapi-proxy will
        # take care of auth.  Everywhere else, we'll get auth failures if
        # necessary.
        if os.path.exists(fn):
            return fn

    def _run_tooltool(self):
        config = self.config
        dirs = self.query_abs_dirs()
        if not config.get('tooltool_manifest_src'):
            return self.warning(ERROR_MSGS['tooltool_manifest_undetermined'])
        fetch_script_path = os.path.join(dirs['abs_tools_dir'],
                                         'scripts/tooltool/tooltool_wrapper.sh')
        tooltool_manifest_path = os.path.join(dirs['abs_mozilla_dir'],
                                              config['tooltool_manifest_src'])
        cmd = [
            'sh',
            fetch_script_path,
            tooltool_manifest_path,
            config['tooltool_url'],
            config['tooltool_bootstrap'],
        ]
        cmd.extend(config['tooltool_script'])
        auth_file = self._get_tooltool_auth_file()
        if auth_file and os.path.exists(auth_file):
            cmd.extend(['--authentication-file', auth_file])
        self.info(str(cmd))
        self.run_command(cmd, cwd=dirs['abs_mozilla_dir'], halt_on_failure=True)

    def funsize_props(self):
        """Set buildbot properties required to trigger funsize tasks
         responsible to generate partial updates for successfully generated locales"""
        funsize_info = {
            'locales': self.query_locales(),
            'branch': self.config['branch'],
            'appName': self.config['appName'],
            'platform': self.config['platform'],
        }
        self.info('funsize info: %s' % funsize_info)
        self.set_buildbot_property('funsize_info', json.dumps(funsize_info),
                                   write_to_file=True)

    def taskcluster_upload(self):
        auth = os.path.join(os.getcwd(), self.config['taskcluster_credentials_file'])
        credentials = {}
        execfile(auth, credentials)
        client_id = credentials.get('taskcluster_clientId')
        access_token = credentials.get('taskcluster_accessToken')
        if not client_id or not access_token:
            self.warning('Skipping S3 file upload: No taskcluster credentials.')
            return

        self.activate_virtualenv()

        # Enable Taskcluster debug logging, so at least we get some debug
        # messages while we are testing uploads.
        logging.getLogger('taskcluster').setLevel(logging.DEBUG)

        branch = self.config['branch']
        platform = self.config['platform']
        revision = self._query_revision()
        repo = self.query_l10n_repo()
        if not repo:
            self.fatal("Unable to determine repository for querying the push info.")
        pushinfo = self.vcs_query_pushinfo(repo, revision, vcs='hgtool')

        routes_json = os.path.join(self.query_abs_dirs()['abs_mozilla_dir'],
                                   'testing/taskcluster/routes.json')
        with open(routes_json) as f:
            contents = json.load(f)
            templates = contents['l10n']

        for locale, files in self.upload_files.iteritems():
            self.info("Uploading files to S3 for locale '%s': %s" % (locale, files))
            routes = []
            for template in templates:
                fmt = {
                    'index': self.config.get('taskcluster_index', 'index.garbage.staging'),
                    'project': branch,
                    'head_rev': revision,
                    'build_product': self.config['stage_product'],
                    'build_name': self.query_build_name(),
                    'build_type': self.query_build_type(),
                    'locale': locale,
                }
                fmt.update(self.buildid_to_dict(self._query_buildid()))
                routes.append(template.format(**fmt))

            self.info('Using routes: %s' % routes)
            tc = Taskcluster(branch,
                             pushinfo.pushdate, # Use pushdate as the rank
                             client_id,
                             access_token,
                             self.log_obj,
                             )
            task = tc.create_task(routes)
            tc.claim_task(task)

            for upload_file in files:
                # Create an S3 artifact for each file that gets uploaded. We also
                # check the uploaded file against the property conditions so that we
                # can set the buildbot config with the correct URLs for package
                # locations.
                tc.create_artifact(task, upload_file)
            tc.report_completed(task)

# main {{{
if __name__ == '__main__':
    single_locale = DesktopSingleLocale()
    single_locale.run_and_exit()
