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
import glob
import re
import sys
import time
import shlex
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

try:
    import simplejson as json
    assert json
except ImportError:
    import json


# needed by _map
SUCCESS = 0
FAILURE = 1

SUCCESS_STR = "Success"
FAILURE_STR = "Failed"


# mandatory configuration options, without them, this script will not work
# it's a list of values that are already known before starting a build
configuration_tokens = ('branch',
                        'platform',
                        'update_platform',
                        'update_channel',
                        'ssh_key_dir',
                        'stage_product',
                        'upload_environment',
                        )
# some other values such as "%(version)s", "%(buildid)s", ...
# are defined at run time and they cannot be enforced in the _pre_config_lock
# phase
runtime_config_tokens = ('buildid', 'version', 'locale', 'from_buildid',
                         'abs_objdir', 'abs_merge_dir', 'revision',
                         'to_buildid', 'en_us_binary_url',
                         'en_us_installer_binary_url', 'mar_tools_url',
                         'post_upload_extra', 'who')


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
         "help": "Specify the locale(s) to sign and update. Optionally pass"
                 " revision separated by colon, en-GB:default."}
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
        ['--revision', ],
        {"action": "store",
         "dest": "revision",
         "type": "string",
         "help": "Override the gecko revision to use (otherwise use buildbot supplied"
                 " value, or en-US revision) "}
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
    ], [
        ["--disable-mock"], {
        "dest": "disable_mock",
        "action": "store_true",
        "help": "do not run under mock despite what gecko-config says"}
    ]]

    def __init__(self, require_config_file=True):
        # fxbuild style:
        buildscript_kwargs = {
            'all_actions': [
                "clobber",
                "pull",
                "clone-locales",
                "list-locales",
                "setup",
                "repack",
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
                    'requests==2.8.1',
                    'PyHawk-with-a-single-extra-commit==0.1.5',
                    'taskcluster==0.0.26',
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
        self.enUS_revision = None
        self.version = None
        self.upload_urls = {}
        self.locales_property = {}
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

        self.read_buildbot_config()
        if not self.buildbot_config:
            self.warning("Skipping buildbot properties overrides")
            # Set an empty dict
            self.buildbot_config = {"properties": {}}
            return
        props = self.buildbot_config["properties"]
        for prop in ['mar_tools_url']:
            if props.get(prop):
                self.info("Overriding %s with %s" % (prop, props[prop]))
                self.config[prop] = props.get(prop)

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
        else:
            # everything else, bool, number, ...
            return config_option

    # Helper methods {{{2
    def query_bootstrap_env(self):
        """returns the env for repacks"""
        if self.bootstrap_env:
            return self.bootstrap_env
        config = self.config
        replace_dict = self.query_abs_dirs()

        replace_dict['en_us_binary_url'] = config.get('en_us_binary_url')
        self.read_buildbot_config()
        # Override en_us_binary_url if packageUrl is passed as a property from
        # the en-US build
        if self.buildbot_config["properties"].get("packageUrl"):
            packageUrl = self.buildbot_config["properties"]["packageUrl"]
            # trim off the filename, the build system wants a directory
            packageUrl = packageUrl.rsplit('/', 1)[0]
            self.info("Overriding en_us_binary_url with %s" % packageUrl)
            replace_dict['en_us_binary_url'] = str(packageUrl)
        # Override en_us_binary_url if passed as a buildbot property
        if self.buildbot_config["properties"].get("en_us_binary_url"):
            self.info("Overriding en_us_binary_url with %s" %
                      self.buildbot_config["properties"]["en_us_binary_url"])
            replace_dict['en_us_binary_url'] = \
                str(self.buildbot_config["properties"]["en_us_binary_url"])
        bootstrap_env = self.query_env(partial_env=config.get("bootstrap_env"),
                                       replace_dict=replace_dict)
        # Override en_us_installer_binary_url if passed as a buildbot property
        if self.buildbot_config["properties"].get("en_us_installer_binary_url"):
            self.info("Overriding en_us_binary_url with %s" %
                      self.buildbot_config["properties"]["en_us_installer_binary_url"])
            bootstrap_env['EN_US_INSTALLER_BINARY_URL'] = str(
                self.buildbot_config["properties"]["en_us_installer_binary_url"])
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
            if 'LOCALE_MERGEDIR' in bootstrap_env:
                # windows fix
                bootstrap_env['LOCALE_MERGEDIR'] = bootstrap_env['LOCALE_MERGEDIR'].replace('\\', '\\\\\\\\')
        if self.query_is_nightly():
            bootstrap_env["IS_NIGHTLY"] = "yes"
        self.bootstrap_env = bootstrap_env
        return self.bootstrap_env

    def _query_upload_env(self):
        """returns the environment used for the upload step"""
        if self.upload_env:
            return self.upload_env
        config = self.config

        replace_dict = {
            'buildid': self._query_buildid(),
            'version': self.query_version(),
            'post_upload_extra': ' '.join(config.get('post_upload_extra', [])),
            'upload_environment': config['upload_environment'],
        }
        if config['branch'] == 'try':
            replace_dict.update({
                'who': self.query_who(),
                'revision': self._query_revision(),
            })
        upload_env = self.query_env(partial_env=config.get("upload_env"),
                                    replace_dict=replace_dict)
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
        """ Get the gecko revision in this order of precedence
              * cached value
              * command line arg --revision   (development, taskcluster)
              * buildbot properties           (try with buildbot forced build)
              * buildbot change               (try with buildbot scheduler)
              * from the en-US build          (m-c & m-a)

        This will fail the last case if the build hasn't been pulled yet.
        """
        if self.revision:
            return self.revision

        self.read_buildbot_config()
        config = self.config
        revision = None
        if config.get("revision"):
            revision = config["revision"]
        elif 'revision' in self.buildbot_properties:
            revision = self.buildbot_properties['revision']
        elif (self.buildbot_config and
                  self.buildbot_config.get('sourcestamp', {}).get('revision')):
            revision = self.buildbot_config['sourcestamp']['revision']
        elif self.buildbot_config and self.buildbot_config.get('revision'):
            revision = self.buildbot_config['revision']
        elif config.get("update_gecko_source_to_enUS", True):
            revision = self._query_enUS_revision()

        if not revision:
            self.fatal("Can't determine revision!")
        self.revision = str(revision)
        return self.revision

    def _query_enUS_revision(self):
        """Get revision from the objdir.
        Only valid after setup is run.
       """
        if self.enUS_revision:
            return self.enUS_revision
        r = re.compile(r"^(gecko|fx)_revision ([0-9a-f]+\+?)$")
        output = self._query_make_ident_output()
        for line in output.splitlines():
            match = r.match(line)
            if match:
                self.enUS_revision = match.groups()[1]
        return self.enUS_revision

    def _query_make_variable(self, variable, make_args=None):
        """returns the value of make echo-variable-<variable>
           it accepts extra make arguements (make_args)
        """
        dirs = self.query_abs_dirs()
        make_args = make_args or []
        target = ["echo-variable-%s" % variable] + make_args
        cwd = dirs['abs_locales_dir']
        raw_output = self._get_output_from_make(target, cwd=cwd,
                                                env=self.query_bootstrap_env())
        # we want to log all the messages from make
        output = []
        for line in raw_output.split("\n"):
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
        self.locales_property[locale] = FAILURE_STR
        prop_key = "%s_failure" % locale
        prop_value = self.query_buildbot_property(prop_key)
        if prop_value:
            prop_value = "%s  %s" % (prop_value, message)
        else:
            prop_value = message
        self.set_buildbot_property(prop_key, prop_value, write_to_file=True)
        BaseScript.add_failure(self, locale, message=message, **kwargs)

    def query_failed_locales(self):
        return [l for l, res in self.locales_property.items() if
                res == FAILURE_STR]

    def summary(self):
        """generates a summary"""
        BaseScript.summary(self)
        # TODO we probably want to make this configurable on/off
        locales = self.query_locales()
        for locale in locales:
            self.locales_property.setdefault(locale, SUCCESS_STR)
        self.set_buildbot_property("locales",
                                   json.dumps(self.locales_property),
                                   write_to_file=True)

    # Actions {{{2
    def clobber(self):
        """clobber"""
        dirs = self.query_abs_dirs()
        clobber_dirs = (dirs['abs_objdir'], dirs['abs_upload_dir'])
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
        # this is OK so early because we get it from buildbot, or
        # the command line for local dev
        replace_dict['revision'] = self._query_revision()

        for repository in config['repos']:
            current_repo = {}
            for key, value in repository.iteritems():
                try:
                    current_repo[key] = value % replace_dict
                except TypeError:
                    # pass through non-interpolables, like booleans
                    current_repo[key] = value
                except KeyError:
                    self.error('not all the values in "{0}" can be replaced. Check your configuration'.format(value))
                    raise
            repos.append(current_repo)
        self.info("repositories: %s" % repos)
        self.vcs_checkout_repos(repos, parent_dir=dirs['abs_work_dir'],
                                tag_override=config.get('tag_override'))

    def clone_locales(self):
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
        self.download_mar_tools()

        # on try we want the source we already have, otherwise update to the
        # same as the en-US binary
        if self.config.get("update_gecko_source_to_enUS", True):
            revision = self._query_enUS_revision()
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
        self.read_from_file(dst, verbose=True)

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
        python = sys.executable
        # A mock environment is a special case, the system python isn't
        # available there
        if 'mock_target' in self.config:
            python = 'python2.7'
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

    def _get_output_from_make(self, target, cwd, env, halt_on_failure=True, ignore_errors=False):
        """runs make and returns the output of the command"""
        make = self._get_make_executable()
        return self.get_output_from_command(make + target,
                                            cwd=cwd,
                                            env=env,
                                            silent=True,
                                            halt_on_failure=halt_on_failure,
                                            ignore_errors=ignore_errors)

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
        return self._make(target=["wget-en-US"], cwd=cwd, env=env)

    def make_upload(self, locale):
        """wrapper for make upload command"""
        config = self.config
        env = self.query_l10n_env()
        dirs = self.query_abs_dirs()
        buildid = self._query_buildid()
        replace_dict = {
            'buildid': buildid,
            'branch': config['branch']
        }
        try:
            env['POST_UPLOAD_CMD'] = config['base_post_upload_cmd'] % replace_dict
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
        if retval == SUCCESS:
            self.info('Upload successful (%s)' % locale)
            ret = SUCCESS
        else:
            self.error('failed to upload %s' % locale)
            ret = FAILURE

        if ret == FAILURE:
            # If we failed above, we shouldn't even attempt a SIMPLE_NAME move
            # even if we are configured to do so
            return ret

        # XXX Move the files to a SIMPLE_NAME format until we can enable
        #     Simple names in the build system
        if self.config.get("simple_name_move"):
            # Assume an UPLOAD PATH
            upload_target = self.config["upload_env"]["UPLOAD_PATH"]
            target_path = os.path.join(upload_target, locale)
            self.mkdir_p(target_path)
            glob_name = "*.%s.*" % locale
            matches = (glob.glob(os.path.join(upload_target, glob_name)) +
                       glob.glob(os.path.join(upload_target, 'update', glob_name)) +
                       glob.glob(os.path.join(upload_target, '*', 'xpi', glob_name)) +
                       glob.glob(os.path.join(upload_target, 'install', 'sea', glob_name)) +
                       glob.glob(os.path.join(upload_target, 'setup.exe')) +
                       glob.glob(os.path.join(upload_target, 'setup-stub.exe')))
            targets_exts = ["tar.bz2", "dmg", "langpack.xpi",
                            "complete.mar", "checksums", "zip",
                            "installer.exe", "installer-stub.exe"]
            targets = ["target.%s" % ext for ext in targets_exts]
            targets.extend(['setup.exe', 'setup-stub.exe'])
            for f in matches:
                target_file = next(target_file for target_file in targets
                                    if f.endswith(target_file[6:]))
                if target_file:
                    # Remove from list of available options for this locale
                    targets.remove(target_file)
                else:
                    # wasn't valid (or already matched)
                    raise RuntimeError("Unexpected matching file name encountered: %s"
                                       % f)
                self.move(os.path.join(f),
                          os.path.join(target_path, target_file))
            self.log("Converted uploads for %s to simple names" % locale)
        return ret

    def set_upload_files(self, locale):
        # The tree doesn't have a good way of exporting the list of files
        # created during locale generation, but we can grab them by echoing the
        # UPLOAD_FILES variable for each locale.
        env = self.query_l10n_env()
        target = ['echo-variable-UPLOAD_FILES', 'echo-variable-CHECKSUM_FILES',
                  'AB_CD=%s' % locale]
        dirs = self.query_abs_dirs()
        cwd = dirs['abs_locales_dir']
        # Bug 1242771 - echo-variable-UPLOAD_FILES via mozharness fails when stderr is found
        #    we should ignore stderr as unfortunately it's expected when parsing for values
        output = self._get_output_from_make(target=target, cwd=cwd, env=env,
                                            ignore_errors=True)
        self.info('UPLOAD_FILES is "%s"' % output)
        files = shlex.split(output)
        if not files:
            self.error('failed to get upload file list for locale %s' % locale)
            return FAILURE

        self.upload_files[locale] = [
            os.path.abspath(os.path.join(cwd, f)) for f in files
        ]
        return SUCCESS

    def make_installers(self, locale):
        """wrapper for make installers-(locale)"""
        env = self.query_l10n_env()
        self._copy_mozconfig()
        dirs = self.query_abs_dirs()
        cwd = os.path.join(dirs['abs_locales_dir'])
        target = ["installers-%s" % locale,
                  "LOCALE_MERGEDIR=%s" % env["LOCALE_MERGEDIR"], ]
        return self._make(target=target, cwd=cwd,
                          env=env, halt_on_failure=False)

    def repack_locale(self, locale):
        """wraps the logic for compare locale, make installers and generating
           complete updates."""

        if self.run_compare_locales(locale) != SUCCESS:
            self.error("compare locale %s failed" % (locale))
            return FAILURE

        # compare locale succeeded, run make installers
        if self.make_installers(locale) != SUCCESS:
            self.error("make installers-%s failed" % (locale))
            return FAILURE

        # now try to upload the artifacts
        if self.make_upload(locale):
            self.error("make upload for locale %s failed!" % (locale))
            return FAILURE

        # set_upload_files() should be called after make upload, to make sure
        # we have all files in place (checksums, etc)
        if self.set_upload_files(locale):
            self.error("failed to get list of files to upload for locale %s" % locale)
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

    def submit_to_balrog(self):
        """submit to balrog"""
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

        # YAY
        def balrog_props_wrapper(locale):
            env = self._query_upload_env()
            props_path = os.path.join(env["UPLOAD_PATH"], locale,
                                      'balrog_props.json')
            self.generate_balrog_props(props_path)
            return SUCCESS

        if self.config.get('taskcluster_nightly'):
            self._map(balrog_props_wrapper, self.query_locales())
        else:
            if not self.config.get("balrog_servers"):
                self.info("balrog_servers not set; skipping balrog submission.")
                return
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
        env = self.query_bootstrap_env()
        config = self.config
        dirs = self.query_abs_dirs()
        manifest_src = os.environ.get('TOOLTOOL_MANIFEST')
        if not manifest_src:
            manifest_src = config.get('tooltool_manifest_src')
        if not manifest_src:
            return
        tooltool_manifest_path = os.path.join(dirs['abs_mozilla_dir'],
                                              manifest_src)
        python = sys.executable
        # A mock environment is a special case, the system python isn't
        # available there
        if 'mock_target' in self.config:
            python = 'python2.7'

        cmd = [
            python, '-u',
            os.path.join(dirs['abs_mozilla_dir'], 'mach'),
            'artifact',
            'toolchain',
            '-v',
            '--retry', '4',
            '--tooltool-manifest',
            tooltool_manifest_path,
            '--tooltool-url',
            config['tooltool_url'],
        ]
        auth_file = self._get_tooltool_auth_file()
        if auth_file and os.path.exists(auth_file):
            cmd.extend(['--authentication-file', auth_file])
        cache = config['bootstrap_env'].get('TOOLTOOL_CACHE')
        if cache:
            cmd.extend(['--cache-dir', cache])
        toolchains = os.environ.get('MOZ_TOOLCHAINS')
        if toolchains:
            cmd.extend(toolchains.split())
        self.info(str(cmd))
        self.run_command(cmd, cwd=dirs['abs_mozilla_dir'], halt_on_failure=True,
                         env=env)

    def funsize_props(self):
        """Set buildbot properties required to trigger funsize tasks
         responsible to generate partial updates for successfully generated locales"""
        locales = self.query_locales()
        funsize_info = {
            'locales': locales,
            'branch': self.config['branch'],
            'appName': self.config['appName'],
            'platform': self.config['platform'],
            'completeMarUrls':  {locale: self._query_complete_mar_url(locale) for locale in locales},
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

        # We need to activate the virtualenv so that we can import taskcluster
        # (and its dependent modules, like requests and hawk).  Normally we
        # could create the virtualenv as an action, but due to some odd
        # dependencies with query_build_env() being called from build(), which
        # is necessary before the virtualenv can be created.
        self.disable_mock()
        self.create_virtualenv()
        self.enable_mock()
        self.activate_virtualenv()

        branch = self.config['branch']
        revision = self._query_revision()
        repo = self.query_l10n_repo()
        if not repo:
            self.fatal("Unable to determine repository for querying the push info.")
        pushinfo = self.vcs_query_pushinfo(repo, revision, vcs='hg')
        pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime(pushinfo.pushdate))

        routes_json = os.path.join(self.query_abs_dirs()['abs_mozilla_dir'],
                                   'testing/mozharness/configs/routes.json')
        with open(routes_json) as f:
            contents = json.load(f)
            templates = contents['l10n']

        # Release promotion creates a special task to accumulate all artifacts
        # under the same task
        artifacts_task = None
        self.read_buildbot_config()
        if "artifactsTaskId" in self.buildbot_config.get("properties", {}):
            artifacts_task_id = self.buildbot_config["properties"]["artifactsTaskId"]
            artifacts_tc = Taskcluster(
                    branch=branch, rank=pushinfo.pushdate, client_id=client_id,
                    access_token=access_token, log_obj=self.log_obj,
                    task_id=artifacts_task_id)
            artifacts_task = artifacts_tc.get_task(artifacts_task_id)
            artifacts_tc.claim_task(artifacts_task)

        for locale, files in self.upload_files.iteritems():
            self.info("Uploading files to S3 for locale '%s': %s" % (locale, files))
            routes = []
            for template in templates:
                fmt = {
                    'index': self.config.get('taskcluster_index', 'index.garbage.staging'),
                    'project': branch,
                    'head_rev': revision,
                    'pushdate': pushdate,
                    'year': pushdate[0:4],
                    'month': pushdate[4:6],
                    'day': pushdate[6:8],
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
                artifact_url = tc.create_artifact(task, upload_file)
                if artifacts_task:
                    artifacts_tc.create_reference_artifact(
                            artifacts_task, upload_file, artifact_url)

            tc.report_completed(task)

        if artifacts_task:
            if not self.query_failed_locales():
                artifacts_tc.report_completed(artifacts_task)
            else:
                # If some locales fail, we want to mark the artifacts
                # task failed, so a retry can reuse the same task ID
                artifacts_tc.report_failed(artifacts_task)


# main {{{
if __name__ == '__main__':
    single_locale = DesktopSingleLocale()
    single_locale.run_and_exit()
