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
import shlex

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import MakefileErrorList
from mozharness.base.script import BaseScript
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.automation import AutomationMixin
from mozharness.mozilla.building.buildbase import (
    MakeUploadOutputParser,
    get_mozconfig_path,
)
from mozharness.mozilla.l10n.locales import LocalesMixin
from mozharness.mozilla.mar import MarMixin
from mozharness.mozilla.release import ReleaseMixin
from mozharness.mozilla.updates.balrog import BalrogMixin
from mozharness.base.python import VirtualenvMixin

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
                        'update_channel',
                        )
# some other values such as "%(version)s", "%(buildid)s", ...
# are defined at run time and they cannot be enforced in the _pre_config_lock
# phase
runtime_config_tokens = ('buildid', 'version', 'locale', 'from_buildid',
                         'abs_objdir', 'revision',
                         'to_buildid', 'en_us_binary_url',
                         'en_us_installer_binary_url', 'mar_tools_url',
                         'who')


# DesktopSingleLocale {{{1
class DesktopSingleLocale(LocalesMixin, ReleaseMixin, AutomationMixin,
                          VCSMixin, BaseScript, BalrogMixin, MarMixin,
                          VirtualenvMixin, TransferMixin):
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
         "help": "Override the gecko revision to use (otherwise use automation supplied"
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
        ['--scm-level'], {  # Ignored on desktop for now: see Bug 1414678.
         "action": "store",
         "type": "int",
         "dest": "scm_level",
         "default": 1,
         "help": "This sets the SCM level for the branch being built."
                 " See https://www.mozilla.org/en-US/about/"
                 "governance/policies/commit/access-policy/"}
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
                "ignore_locales": ["en-US"],
                "locales_dir": "browser/locales",
                "buildid_section": "App",
                "buildid_option": "BuildID",
                "application_ini": "application.ini",
                "log_name": "single_locale",
                "appName": "Firefox",
                "hashType": "sha512",
                'virtualenv_modules': [
                    'requests==2.8.1',
                ],
                'virtualenv_path': 'venv',
            },
        }

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
        self.pushdate = None
        # upload_files is a dictionary of files to upload, keyed by locale.
        self.upload_files = {}

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
        return

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
        # Override en_us_binary_url if packageUrl is passed as a property from
        # the en-US build
        bootstrap_env = self.query_env(partial_env=config.get("bootstrap_env"),
                                       replace_dict=replace_dict)
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

        upload_env = self.query_env(partial_env=config.get("upload_env"))
        # check if there are any extra option from the platform configuration
        # and append them to the env

        if 'upload_env_extra' in config:
            for extra in config['upload_env_extra']:
                upload_env[extra] = config['upload_env_extra'][extra]

        self.upload_env = upload_env
        return self.upload_env

    def query_l10n_env(self):
        l10n_env = self._query_upload_env().copy()
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
        prop_value = self.query_property(prop_key)
        if prop_value:
            prop_value = "%s  %s" % (prop_value, message)
        else:
            prop_value = message
        self.set_property(prop_key, prop_value, write_to_file=True)
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
        self.set_property("locales",
                          json.dumps(self.locales_property),
                          write_to_file=True)

    # Actions {{{2
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
        # this is OK so early because we get it from automation, or
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
                    self.error('not all the values in "{0}" can be replaced. Check your '
                               'configuration'.format(value))
                    raise
            repos.append(current_repo)
        self.info("repositories: %s" % repos)
        self.vcs_checkout_repos(repos, parent_dir=dirs['abs_work_dir'],
                                tag_override=config.get('tag_override'))

    def clone_locales(self):
        self.pull_locale_source()

    def setup(self):
        """setup step"""
        self._run_tooltool()
        self._copy_mozconfig()
        self._mach_configure()
        self._run_make_in_config_dir()
        self.make_wget_en_US()
        self.make_unpack_en_US()
        self.download_mar_tools()

    def _run_make_in_config_dir(self):
        """this step creates nsinstall, needed my make_wget_en_US()
        """
        dirs = self.query_abs_dirs()
        config_dir = os.path.join(dirs['abs_objdir'], 'config')
        env = self.query_bootstrap_env()
        return self._make(target=['export'], cwd=config_dir, env=env)

    def _copy_mozconfig(self):
        """copies the mozconfig file into abs_mozilla_dir/.mozconfig
           and logs the content
        """
        config = self.config
        dirs = self.query_abs_dirs()
        src = get_mozconfig_path(self, config, dirs)
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
        return [sys.executable, 'mach']

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
        env = self.query_l10n_env()
        dirs = self.query_abs_dirs()
        target = ['upload', 'AB_CD=%s' % (locale)]
        cwd = dirs['abs_locales_dir']
        parser = MakeUploadOutputParser(config=self.config,
                                        log_obj=self.log_obj)
        retval = self._make(target=target, cwd=cwd, env=env,
                            halt_on_failure=False, output_parser=parser)
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
            targets = [(".%s" % (ext,), "target.%s" % (ext,)) for ext in targets_exts]
            targets.extend([(f, f) for f in 'setup.exe', 'setup-stub.exe'])
            for f in matches:
                possible_targets = [
                    (tail, target_file)
                    for (tail, target_file) in targets
                    if f.endswith(tail)
                ]
                if len(possible_targets) == 1:
                    _, target_file = possible_targets[0]
                    # Remove from list of available options for this locale
                    targets.remove(possible_targets[0])
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
        target = ["installers-%s" % locale, ]
        return self._make(target=target, cwd=cwd,
                          env=env, halt_on_failure=False)

    def repack_locale(self, locale):
        """wraps the logic for make installers and generating
           complete updates."""

        # run make installers
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
        dirs['abs_src_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'src')
        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def submit_to_balrog(self):
        """submit to balrog"""
        self.info("Reading build properties...")
        # get platform, appName and hashType from configuration
        # common values across different locales
        config = self.config
        platform = config["platform"]
        appName = config['appName']
        branch = config['branch']
        # values from configuration
        self.set_property("branch", branch)
        self.set_property("appName", appName)
        # it's hardcoded to sha512 in balrog.py
        self.set_property("platform", platform)
        # values common to the current repacks
        self.set_property("buildid", self._query_buildid())
        self.set_property("appVersion", self.query_version())

        # YAY
        def balrog_props_wrapper(locale):
            env = self._query_upload_env()
            props_path = os.path.join(env["UPLOAD_PATH"], locale,
                                      'balrog_props.json')
            self.generate_balrog_props(props_path)
            return SUCCESS

        self._map(balrog_props_wrapper, self.query_locales())

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
        toolchains = os.environ.get('MOZ_TOOLCHAINS')
        manifest_src = os.environ.get('TOOLTOOL_MANIFEST')
        if not manifest_src:
            manifest_src = config.get('tooltool_manifest_src')
        if not manifest_src and not toolchains:
            return
        python = sys.executable

        cmd = [
            python, '-u',
            os.path.join(dirs['abs_mozilla_dir'], 'mach'),
            'artifact',
            'toolchain',
            '-v',
            '--retry', '4',
            '--artifact-manifest',
            os.path.join(dirs['abs_mozilla_dir'], 'toolchains.json'),
        ]
        if manifest_src:
            cmd.extend([
                '--tooltool-manifest',
                os.path.join(dirs['abs_mozilla_dir'], manifest_src),
                '--tooltool-url',
                config['tooltool_url'],
            ])
            auth_file = self._get_tooltool_auth_file()
            if auth_file and os.path.exists(auth_file):
                cmd.extend(['--authentication-file', auth_file])
        cache = config['bootstrap_env'].get('TOOLTOOL_CACHE')
        if cache:
            cmd.extend(['--cache-dir', cache])
        if toolchains:
            cmd.extend(toolchains.split())
        self.info(str(cmd))
        self.run_command(cmd, cwd=dirs['abs_mozilla_dir'], halt_on_failure=True,
                         env=env)


# main {{{
if __name__ == '__main__':
    single_locale = DesktopSingleLocale()
    single_locale.run_and_exit()
