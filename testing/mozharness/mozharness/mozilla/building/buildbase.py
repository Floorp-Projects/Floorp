#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" buildbase.py.

provides a base class for fx desktop builds
author: Jordan Lund

"""
import json

import os
import time
import uuid
import copy
import glob

# import the power of mozharness ;)
import sys
from datetime import datetime
import re
from mozharness.base.config import (
    BaseConfig, parse_config_file, DEFAULT_CONFIG_PATH,
)
from mozharness.base.errors import MakefileErrorList
from mozharness.base.log import ERROR, OutputParser, FATAL
from mozharness.base.script import PostScriptRun
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.automation import (
    AutomationMixin,
    EXIT_STATUS_DICT,
    TBPL_STATUS_DICT,
    TBPL_FAILURE,
    TBPL_RETRY,
    TBPL_WARNING,
    TBPL_SUCCESS,
    TBPL_WORST_LEVEL_TUPLE,
)
from mozharness.mozilla.secrets import SecretsMixin
from mozharness.mozilla.testing.errors import TinderBoxPrintRe
from mozharness.mozilla.testing.unittest import tbox_print_summary
from mozharness.base.python import (
    PerfherderResourceOptionsMixin,
    VirtualenvMixin,
)

AUTOMATION_EXIT_CODES = EXIT_STATUS_DICT.values()
AUTOMATION_EXIT_CODES.sort()

MISSING_CFG_KEY_MSG = "The key '%s' could not be determined \
Please add this to your config."

ERROR_MSGS = {
    'comments_undetermined': '"comments" could not be determined. This may be \
because it was a forced build.',
    'tooltool_manifest_undetermined': '"tooltool_manifest_src" not set, \
Skipping run_tooltool...',
}


# Output Parsers

TBPL_UPLOAD_ERRORS = [
    {
        'regex': re.compile("Connection timed out"),
        'level': TBPL_RETRY,
    },
    {
        'regex': re.compile("Connection reset by peer"),
        'level': TBPL_RETRY,
    },
    {
        'regex': re.compile("Connection refused"),
        'level': TBPL_RETRY,
    }
]


class MakeUploadOutputParser(OutputParser):
    tbpl_error_list = TBPL_UPLOAD_ERRORS

    def __init__(self, **kwargs):
        super(MakeUploadOutputParser, self).__init__(**kwargs)
        self.tbpl_status = TBPL_SUCCESS

    def parse_single_line(self, line):
        # let's check for retry errors which will give log levels:
        # tbpl status as RETRY and mozharness status as WARNING
        for error_check in self.tbpl_error_list:
            if error_check['regex'].search(line):
                self.num_warnings += 1
                self.warning(line)
                self.tbpl_status = self.worst_level(
                    error_check['level'], self.tbpl_status,
                    levels=TBPL_WORST_LEVEL_TUPLE
                )
                break
        else:
            self.info(line)


class CheckTestCompleteParser(OutputParser):
    tbpl_error_list = TBPL_UPLOAD_ERRORS

    def __init__(self, **kwargs):
        self.matches = {}
        super(CheckTestCompleteParser, self).__init__(**kwargs)
        self.pass_count = 0
        self.fail_count = 0
        self.leaked = False
        self.harness_err_re = TinderBoxPrintRe['harness_error']['full_regex']
        self.tbpl_status = TBPL_SUCCESS

    def parse_single_line(self, line):
        # Counts and flags.
        # Regular expression for crash and leak detections.
        if "TEST-PASS" in line:
            self.pass_count += 1
            return self.info(line)
        if "TEST-UNEXPECTED-" in line:
            # Set the error flags.
            # Or set the failure count.
            m = self.harness_err_re.match(line)
            if m:
                r = m.group(1)
                if r == "missing output line for total leaks!":
                    self.leaked = None
                else:
                    self.leaked = True
            self.fail_count += 1
            return self.warning(line)
        self.info(line)  # else

    def evaluate_parser(self, return_code,  success_codes=None):
        success_codes = success_codes or [0]

        if self.num_errors:  # ran into a script error
            self.tbpl_status = self.worst_level(TBPL_FAILURE, self.tbpl_status,
                                                levels=TBPL_WORST_LEVEL_TUPLE)

        if self.fail_count > 0:
            self.tbpl_status = self.worst_level(TBPL_WARNING, self.tbpl_status,
                                                levels=TBPL_WORST_LEVEL_TUPLE)

        # Account for the possibility that no test summary was output.
        if (self.pass_count == 0 and self.fail_count == 0 and
           os.environ.get('TRY_SELECTOR') != 'coverage'):
            self.error('No tests run or test summary not found')
            self.tbpl_status = self.worst_level(TBPL_WARNING, self.tbpl_status,
                                                levels=TBPL_WORST_LEVEL_TUPLE)

        if return_code not in success_codes:
            self.tbpl_status = self.worst_level(TBPL_FAILURE, self.tbpl_status,
                                                levels=TBPL_WORST_LEVEL_TUPLE)

        # Print the summary.
        summary = tbox_print_summary(self.pass_count,
                                     self.fail_count,
                                     self.leaked)
        self.info("TinderboxPrint: check<br/>%s\n" % summary)

        return self.tbpl_status


class MozconfigPathError(Exception):
    """
    There was an error getting a mozconfig path from a mozharness config.
    """


def get_mozconfig_path(script, config, dirs):
    """
    Get the path to the mozconfig file to use from a mozharness config.

    :param script: The object to interact with the filesystem through.
    :type script: ScriptMixin:

    :param config: The mozharness config to inspect.
    :type config: dict

    :param dirs: The directories specified for this build.
    :type dirs: dict
    """
    COMPOSITE_KEYS = {'mozconfig_variant', 'app_name', 'mozconfig_platform'}
    have_composite_mozconfig = COMPOSITE_KEYS <= set(config.keys())
    have_partial_composite_mozconfig = len(COMPOSITE_KEYS & set(config.keys())) > 0
    have_src_mozconfig = 'src_mozconfig' in config
    have_src_mozconfig_manifest = 'src_mozconfig_manifest' in config

    # first determine the mozconfig path
    if have_partial_composite_mozconfig and not have_composite_mozconfig:
        raise MozconfigPathError(
            "All or none of 'app_name', 'mozconfig_platform' and `mozconfig_variant' must be "
            "in the config in order to determine the mozconfig.")
    elif have_composite_mozconfig and have_src_mozconfig:
        raise MozconfigPathError(
            "'src_mozconfig' or 'mozconfig_variant' must be "
            "in the config but not both in order to determine the mozconfig.")
    elif have_composite_mozconfig and have_src_mozconfig_manifest:
        raise MozconfigPathError(
            "'src_mozconfig_manifest' or 'mozconfig_variant' must be "
            "in the config but not both in order to determine the mozconfig.")
    elif have_src_mozconfig and have_src_mozconfig_manifest:
        raise MozconfigPathError(
            "'src_mozconfig' or 'src_mozconfig_manifest' must be "
            "in the config but not both in order to determine the mozconfig.")
    elif have_composite_mozconfig:
        src_mozconfig = '%(app_name)s/config/mozconfigs/%(platform)s/%(variant)s' % {
            'app_name': config['app_name'],
            'platform': config['mozconfig_platform'],
            'variant': config['mozconfig_variant'],
        }
        abs_mozconfig_path = os.path.join(dirs['abs_src_dir'], src_mozconfig)
    elif have_src_mozconfig:
        abs_mozconfig_path = os.path.join(dirs['abs_src_dir'], config.get('src_mozconfig'))
    elif have_src_mozconfig_manifest:
        manifest = os.path.join(dirs['abs_work_dir'], config['src_mozconfig_manifest'])
        if not os.path.exists(manifest):
            raise MozconfigPathError(
                'src_mozconfig_manifest: "%s" not found. Does it exist?' % (manifest,))
        else:
            with script.opened(manifest, error_level=ERROR) as (fh, err):
                if err:
                    raise MozconfigPathError("%s exists but coud not read properties" % manifest)
                abs_mozconfig_path = os.path.join(dirs['abs_src_dir'], json.load(fh)['gecko_path'])
    else:
        raise MozconfigPathError(
            "Must provide 'app_name', 'mozconfig_platform' and 'mozconfig_variant'; "
            "or one of 'src_mozconfig' or 'src_mozconfig_manifest' in the config "
            "in order to determine the mozconfig.")

    return abs_mozconfig_path


class BuildingConfig(BaseConfig):
    # TODO add nosetests for this class
    def get_cfgs_from_files(self, all_config_files, options):
        """
        Determine the configuration from the normal options and from
        `--branch`, `--build-pool`, and `--custom-build-variant-cfg`.  If the
        files for any of the latter options are also given with `--config-file`
        or `--opt-config-file`, they are only parsed once.

        The build pool has highest precedence, followed by branch, build
        variant, and any normally-specified configuration files.
        """
        # override from BaseConfig

        # this is what we will return. It will represent each config
        # file name and its associated dict
        # eg ('builds/branch_specifics.py', {'foo': 'bar'})
        all_config_dicts = []
        # important config files
        variant_cfg_file = pool_cfg_file = ''

        # we want to make the order in which the options were given
        # not matter. ie: you can supply --branch before --build-pool
        # or vice versa and the hierarchy will not be different

        # ### The order from highest precedence to lowest is:
        # # There can only be one of these...
        # 1) build_pool: this can be either staging, pre-prod, and prod cfgs
        # 2) build_variant: these could be known like asan and debug
        #                   or a custom config
        #
        # # There can be many of these
        # 3) all other configs: these are any configs that are passed with
        #                       --cfg and --opt-cfg. There order is kept in
        #                       which they were passed on the cmd line. This
        #                       behaviour is maintains what happens by default
        #                       in mozharness

        # so, let's first assign the configs that hold a known position of
        # importance (1 through 3)
        for i, cf in enumerate(all_config_files):
            if options.build_pool:
                if cf == BuildOptionParser.build_pool_cfg_file:
                    pool_cfg_file = all_config_files[i]

            if cf == options.build_variant:
                variant_cfg_file = all_config_files[i]

        # now remove these from the list if there was any.
        # we couldn't pop() these in the above loop as mutating a list while
        # iterating through it causes spurious results :)
        for cf in [pool_cfg_file, variant_cfg_file]:
            if cf:
                all_config_files.remove(cf)

        # now let's update config with the remaining config files.
        # this functionality is the same as the base class
        all_config_dicts.extend(
            super(BuildingConfig, self).get_cfgs_from_files(all_config_files,
                                                            options)
        )

        # stack variant, branch, and pool cfg files on top of that,
        # if they are present, in that order
        if variant_cfg_file:
            # take the whole config
            all_config_dicts.append(
                (variant_cfg_file, parse_config_file(variant_cfg_file))
            )
        config_paths = options.config_paths or ['.']
        if pool_cfg_file:
            # take only the specific pool. If we are here, the pool
            # must be present
            build_pool_configs = parse_config_file(
                pool_cfg_file,
                search_path=config_paths + [DEFAULT_CONFIG_PATH])
            all_config_dicts.append(
                (pool_cfg_file, build_pool_configs[options.build_pool])
            )
        return all_config_dicts


# noinspection PyUnusedLocal
class BuildOptionParser(object):
    # TODO add nosetests for this class
    platform = None
    bits = None

    # add to this list and you can automagically do things like
    # --custom-build-variant-cfg asan
    # and the script will pull up the appropriate path for the config
    # against the current platform and bits.
    # *It will warn and fail if there is not a config for the current
    # platform/bits
    build_variants = {
        'add-on-devel': 'builds/releng_sub_%s_configs/%s_add-on-devel.py',
        'asan': 'builds/releng_sub_%s_configs/%s_asan.py',
        'asan-tc': 'builds/releng_sub_%s_configs/%s_asan_tc.py',
        'asan-reporter-tc': 'builds/releng_sub_%s_configs/%s_asan_reporter_tc.py',
        'fuzzing-asan-tc': 'builds/releng_sub_%s_configs/%s_fuzzing_asan_tc.py',
        'tsan': 'builds/releng_sub_%s_configs/%s_tsan.py',
        'cross-debug': 'builds/releng_sub_%s_configs/%s_cross_debug.py',
        'cross-debug-searchfox': 'builds/releng_sub_%s_configs/%s_cross_debug_searchfox.py',
        'cross-noopt-debug': 'builds/releng_sub_%s_configs/%s_cross_noopt_debug.py',
        'cross-fuzzing-asan': 'builds/releng_sub_%s_configs/%s_cross_fuzzing_asan.py',
        'cross-fuzzing-debug': 'builds/releng_sub_%s_configs/%s_cross_fuzzing_debug.py',
        'debug': 'builds/releng_sub_%s_configs/%s_debug.py',
        'fuzzing-debug': 'builds/releng_sub_%s_configs/%s_fuzzing_debug.py',
        'asan-and-debug': 'builds/releng_sub_%s_configs/%s_asan_and_debug.py',
        'asan-tc-and-debug': 'builds/releng_sub_%s_configs/%s_asan_tc_and_debug.py',
        'stat-and-debug': 'builds/releng_sub_%s_configs/%s_stat_and_debug.py',
        'code-coverage-debug': 'builds/releng_sub_%s_configs/%s_code_coverage_debug.py',
        'code-coverage-opt': 'builds/releng_sub_%s_configs/%s_code_coverage_opt.py',
        'source': 'builds/releng_sub_%s_configs/%s_source.py',
        'noopt-debug': 'builds/releng_sub_%s_configs/%s_noopt_debug.py',
        'api-16-gradle-dependencies':
            'builds/releng_sub_%s_configs/%s_api_16_gradle_dependencies.py',
        'api-16': 'builds/releng_sub_%s_configs/%s_api_16.py',
        'api-16-beta': 'builds/releng_sub_%s_configs/%s_api_16_beta.py',
        'api-16-beta-debug': 'builds/releng_sub_%s_configs/%s_api_16_beta_debug.py',
        'api-16-debug': 'builds/releng_sub_%s_configs/%s_api_16_debug.py',
        'api-16-debug-ccov': 'builds/releng_sub_%s_configs/%s_api_16_debug_ccov.py',
        'api-16-debug-searchfox': 'builds/releng_sub_%s_configs/%s_api_16_debug_searchfox.py',
        'api-16-gradle': 'builds/releng_sub_%s_configs/%s_api_16_gradle.py',
        'api-16-profile-generate': 'builds/releng_sub_%s_configs/%s_api_16_profile_generate.py',
        'api-16-profile-use': 'builds/releng_sub_%s_configs/%s_api_16_profile_use.py',
        'api-16-release': 'builds/releng_sub_%s_configs/%s_api_16_release.py',
        'api-16-release-debug': 'builds/releng_sub_%s_configs/%s_api_16_release_debug.py',
        'api-16-without-google-play-services':
            'builds/releng_sub_%s_configs/%s_api_16_without_google_play_services.py',
        'rusttests': 'builds/releng_sub_%s_configs/%s_rusttests.py',
        'rusttests-debug': 'builds/releng_sub_%s_configs/%s_rusttests_debug.py',
        'x86': 'builds/releng_sub_%s_configs/%s_x86.py',
        'x86-beta': 'builds/releng_sub_%s_configs/%s_x86_beta.py',
        'x86-beta-debug': 'builds/releng_sub_%s_configs/%s_x86_beta_debug.py',
        'x86-debug': 'builds/releng_sub_%s_configs/%s_x86_debug.py',
        'x86-fuzzing-debug': 'builds/releng_sub_%s_configs/%s_x86_fuzzing_debug.py',
        'x86-release': 'builds/releng_sub_%s_configs/%s_x86_release.py',
        'x86-release-debug': 'builds/releng_sub_%s_configs/%s_x86_release_debug.py',
        'x86_64': 'builds/releng_sub_%s_configs/%s_x86_64.py',
        'x86_64-beta': 'builds/releng_sub_%s_configs/%s_x86_64_beta.py',
        'x86_64-beta-debug': 'builds/releng_sub_%s_configs/%s_x86_64_beta_debug.py',
        'x86_64-debug': 'builds/releng_sub_%s_configs/%s_x86_64_debug.py',
        'x86_64-release': 'builds/releng_sub_%s_configs/%s_x86_64_release.py',
        'x86_64-release-debug': 'builds/releng_sub_%s_configs/%s_x86_64_release_debug.py',
        'api-16-partner-sample1': 'builds/releng_sub_%s_configs/%s_api_16_partner_sample1.py',
        'aarch64': 'builds/releng_sub_%s_configs/%s_aarch64.py',
        'aarch64-beta': 'builds/releng_sub_%s_configs/%s_aarch64_beta.py',
        'aarch64-beta-debug': 'builds/releng_sub_%s_configs/%s_aarch64_beta_debug.py',
        'aarch64-debug': 'builds/releng_sub_%s_configs/%s_aarch64_debug.py',
        'aarch64-release': 'builds/releng_sub_%s_configs/%s_aarch64_release.py',
        'aarch64-release-debug': 'builds/releng_sub_%s_configs/%s_aarch64_release_debug.py',
        'android-test': 'builds/releng_sub_%s_configs/%s_test.py',
        'android-test-ccov': 'builds/releng_sub_%s_configs/%s_test_ccov.py',
        'android-checkstyle': 'builds/releng_sub_%s_configs/%s_checkstyle.py',
        'android-api-lint': 'builds/releng_sub_%s_configs/%s_api_lint.py',
        'android-lint': 'builds/releng_sub_%s_configs/%s_lint.py',
        'android-findbugs': 'builds/releng_sub_%s_configs/%s_findbugs.py',
        'android-geckoview-docs': 'builds/releng_sub_%s_configs/%s_geckoview_docs.py',
        'valgrind': 'builds/releng_sub_%s_configs/%s_valgrind.py',
        'tup': 'builds/releng_sub_%s_configs/%s_tup.py',
    }
    build_pool_cfg_file = 'builds/build_pool_specifics.py'

    @classmethod
    def _query_pltfrm_and_bits(cls, target_option, options):
        """ determine platform and bits

        This can be from either from a supplied --platform and --bits
        or parsed from given config file names.
        """
        error_msg = (
            'Whoops!\nYou are trying to pass a shortname for '
            '%s. \nHowever, I need to know the %s to find the appropriate '
            'filename. You can tell me by passing:\n\t"%s" or a config '
            'filename via "--config" with %s in it. \nIn either case, these '
            'option arguments must come before --custom-build-variant.'
        )
        current_config_files = options.config_files or []
        if not cls.bits:
            # --bits has not been supplied
            # lets parse given config file names for 32 or 64
            for cfg_file_name in current_config_files:
                if '32' in cfg_file_name:
                    cls.bits = '32'
                    break
                if '64' in cfg_file_name:
                    cls.bits = '64'
                    break
            else:
                sys.exit(error_msg % (target_option, 'bits', '--bits',
                                      '"32" or "64"'))

        if not cls.platform:
            # --platform has not been supplied
            # lets parse given config file names for platform
            for cfg_file_name in current_config_files:
                if 'windows' in cfg_file_name:
                    cls.platform = 'windows'
                    break
                if 'mac' in cfg_file_name:
                    cls.platform = 'mac'
                    break
                if 'linux' in cfg_file_name:
                    cls.platform = 'linux'
                    break
                if 'android' in cfg_file_name:
                    cls.platform = 'android'
                    break
            else:
                sys.exit(error_msg % (target_option, 'platform', '--platform',
                                      '"linux", "windows", "mac", or "android"'))
        return cls.bits, cls.platform

    @classmethod
    def find_variant_cfg_path(cls, opt, value, parser):
        valid_variant_cfg_path = None
        # first let's see if we were given a valid short-name
        if cls.build_variants.get(value):
            bits, pltfrm = cls._query_pltfrm_and_bits(opt, parser.values)
            prospective_cfg_path = cls.build_variants[value] % (pltfrm, bits)
        else:
            # this is either an incomplete path or an invalid key in
            # build_variants
            prospective_cfg_path = value

        if os.path.exists(prospective_cfg_path):
            # now let's see if we were given a valid pathname
            valid_variant_cfg_path = value
        else:
            # FIXME: We should actually wait until we have parsed all arguments
            # before looking at this, otherwise the behavior will depend on the
            # order of arguments. But that isn't a problem as long as --extra-config-path
            # is always passed first.
            extra_config_paths = parser.values.config_paths or []
            config_paths = extra_config_paths + [DEFAULT_CONFIG_PATH]
            # let's take our prospective_cfg_path and see if we can
            # determine an existing file
            for path in config_paths:
                if os.path.exists(os.path.join(path, prospective_cfg_path)):
                    # success! we found a config file
                    valid_variant_cfg_path = os.path.join(path,
                                                          prospective_cfg_path)
                    break
        return valid_variant_cfg_path, prospective_cfg_path

    @classmethod
    def set_build_variant(cls, option, opt, value, parser):
        """ sets an extra config file.

        This is done by either taking an existing filepath or by taking a valid
        shortname coupled with known platform/bits.
        """
        valid_variant_cfg_path, prospective_cfg_path = cls.find_variant_cfg_path(
            '--custom-build-variant-cfg', value, parser)

        if not valid_variant_cfg_path:
            # either the value was an indeterminable path or an invalid short
            # name
            sys.exit("Whoops!\n'--custom-build-variant' was passed but an "
                     "appropriate config file could not be determined. Tried "
                     "using: '%s' but it was not:"
                     "\n\t-- a valid shortname: %s "
                     "\n\t-- a valid variant for the given platform and bits." % (
                         prospective_cfg_path,
                         str(cls.build_variants.keys())))
        parser.values.config_files.append(valid_variant_cfg_path)
        setattr(parser.values, option.dest, value)  # the pool

    @classmethod
    def set_build_pool(cls, option, opt, value, parser):
        # first let's add the build pool file where there may be pool
        # specific keys/values. Then let's store the pool name
        parser.values.config_files.append(cls.build_pool_cfg_file)
        setattr(parser.values, option.dest, value)  # the pool

    @classmethod
    def set_build_branch(cls, option, opt, value, parser):
        # Store the branch name we are using
        setattr(parser.values, option.dest, value)  # the branch name

    @classmethod
    def set_platform(cls, option, opt, value, parser):
        cls.platform = value
        setattr(parser.values, option.dest, value)

    @classmethod
    def set_bits(cls, option, opt, value, parser):
        cls.bits = value
        setattr(parser.values, option.dest, value)


# this global depends on BuildOptionParser and therefore can not go at the
# top of the file
BUILD_BASE_CONFIG_OPTIONS = [
    [['--developer-run'], {
        "action": "store_false",
        "dest": "is_automation",
        "default": True,
        "help": "If this is running outside of Mozilla's build"
                "infrastructure, use this option. It ignores actions"
                "that are not needed and adds config checks."}],
    [['--platform'], {
        "action": "callback",
        "callback": BuildOptionParser.set_platform,
        "type": "string",
        "dest": "platform",
        "help": "Sets the platform we are running this against"
                " valid values: 'windows', 'mac', 'linux'"}],
    [['--bits'], {
        "action": "callback",
        "callback": BuildOptionParser.set_bits,
        "type": "string",
        "dest": "bits",
        "help": "Sets which bits we are building this against"
                " valid values: '32', '64'"}],
    [['--custom-build-variant-cfg'], {
        "action": "callback",
        "callback": BuildOptionParser.set_build_variant,
        "type": "string",
        "dest": "build_variant",
        "help": "Sets the build type and will determine appropriate"
                " additional config to use. Either pass a config path"
                " or use a valid shortname from: "
                "%s" % (BuildOptionParser.build_variants.keys(),)}],
    [['--build-pool'], {
        "action": "callback",
        "callback": BuildOptionParser.set_build_pool,
        "type": "string",
        "dest": "build_pool",
        "help": "This will update the config with specific pool"
                " environment keys/values. The dicts for this are"
                " in %s\nValid values: staging or"
                " production" % ('builds/build_pool_specifics.py',)}],
    [['--branch'], {
        "action": "callback",
        "callback": BuildOptionParser.set_build_branch,
        "type": "string",
        "dest": "branch",
        "help": "This sets the branch we will be building this for."}],
    [['--scm-level'], {
        "action": "store",
        "type": "int",
        "dest": "scm_level",
        "default": 1,
        "help": "This sets the SCM level for the branch being built."
                " See https://www.mozilla.org/en-US/about/"
                "governance/policies/commit/access-policy/"}],
    [['--enable-pgo'], {
        "action": "store_true",
        "dest": "pgo_build",
        "default": False,
        "help": "Sets the build to run in PGO mode"}],
    [['--enable-nightly'], {
        "action": "store_true",
        "dest": "nightly_build",
        "default": False,
        "help": "Sets the build to run in nightly mode"}],
    [['--who'], {
        "dest": "who",
        "default": '',
        "help": "stores who made the created the change."}],
]


def generate_build_ID():
    return time.strftime("%Y%m%d%H%M%S", time.localtime(time.time()))


def generate_build_UID():
    return uuid.uuid4().hex


class BuildScript(AutomationMixin,
                  VirtualenvMixin, MercurialScript,
                  SecretsMixin, PerfherderResourceOptionsMixin):
    def __init__(self, **kwargs):
        # objdir is referenced in _query_abs_dirs() so let's make sure we
        # have that attribute before calling BaseScript.__init__
        self.objdir = None
        super(BuildScript, self).__init__(**kwargs)
        # epoch is only here to represent the start of the build
        # that this mozharn script came from. until I can grab bbot's
        # status.build.gettime()[0] this will have to do as a rough estimate
        # although it is about 4s off from the time it would be if it was
        # done through MBF.
        # TODO find out if that time diff matters or if we just use it to
        # separate each build
        self.epoch_timestamp = int(time.mktime(datetime.now().timetuple()))
        self.branch = self.config.get('branch')
        self.stage_platform = self.config.get('stage_platform')
        if not self.branch or not self.stage_platform:
            if not self.branch:
                self.error("'branch' not determined and is required")
            if not self.stage_platform:
                self.error("'stage_platform' not determined and is required")
            self.fatal("Please add missing items to your config")
        self.buildid = None
        self.query_buildid()  # sets self.buildid
        self.generated_build_props = False
        self.client_id = None
        self.access_token = None

        # Call this before creating the virtualenv so that we can support
        # substituting config values with other config values.
        self.query_build_env()

        # We need to create the virtualenv directly (without using an action) in
        # order to use python modules in PreScriptRun/Action listeners
        self.create_virtualenv()

    def _pre_config_lock(self, rw_config):
        c = self.config
        cfg_files_and_dicts = rw_config.all_cfg_files_and_dicts
        build_pool = c.get('build_pool', '')
        build_variant = c.get('build_variant', '')
        variant_cfg = ''
        if build_variant:
            variant_cfg = BuildOptionParser.build_variants[build_variant] % (
                BuildOptionParser.platform,
                BuildOptionParser.bits
            )
        build_pool_cfg = BuildOptionParser.build_pool_cfg_file

        cfg_match_msg = "Script was run with '%(option)s %(type)s' and \
'%(type)s' matches a key in '%(type_config_file)s'. Updating self.config with \
items from that key's value."

        for i, (target_file, target_dict) in enumerate(cfg_files_and_dicts):
            if build_pool_cfg and build_pool_cfg in target_file:
                self.info(
                    cfg_match_msg % {
                        'option': '--build-pool',
                        'type': build_pool,
                        'type_config_file': build_pool_cfg,
                    }
                )
            if variant_cfg and variant_cfg in target_file:
                self.info(
                    cfg_match_msg % {
                        'option': '--custom-build-variant-cfg',
                        'type': build_variant,
                        'type_config_file': variant_cfg,
                    }
                )
        self.info('To generate a config file based upon options passed and '
                  'config files used, run script as before but extend options '
                  'with "--dump-config"')
        self.info('For a diff of where self.config got its items, '
                  'run the script again as before but extend options with: '
                  '"--dump-config-hierarchy"')
        self.info("Both --dump-config and --dump-config-hierarchy don't "
                  "actually run any actions.")

    def _assert_cfg_valid_for_action(self, dependencies, action):
        """ assert dependency keys are in config for given action.

        Takes a list of dependencies and ensures that each have an
        assoctiated key in the config. Displays error messages as
        appropriate.

        """
        # TODO add type and value checking, not just keys
        # TODO solution should adhere to: bug 699343
        # TODO add this to BaseScript when the above is done
        # for now, let's just use this as a way to save typing...
        c = self.config
        undetermined_keys = []
        err_template = "The key '%s' could not be determined \
and is needed for the action '%s'. Please add this to your config \
or run without that action (ie: --no-{action})"
        for dep in dependencies:
            if dep not in c:
                undetermined_keys.append(dep)
        if undetermined_keys:
            fatal_msgs = [err_template % (key, action)
                          for key in undetermined_keys]
            self.fatal("".join(fatal_msgs))
        # otherwise:
        return  # all good

    def _query_build_prop_from_app_ini(self, prop, app_ini_path=None):
        dirs = self.query_abs_dirs()
        print_conf_setting_path = os.path.join(dirs['abs_src_dir'],
                                               'config',
                                               'printconfigsetting.py')
        if not app_ini_path:
            # set the default
            app_ini_path = dirs['abs_app_ini_path']
        if (os.path.exists(print_conf_setting_path) and
                os.path.exists(app_ini_path)):
            cmd = [
                sys.executable, os.path.join(dirs['abs_src_dir'], 'mach'), 'python',
                print_conf_setting_path, app_ini_path,
                'App', prop
            ]
            env = self.query_build_env()
            # dirs['abs_obj_dir'] can be different from env['MOZ_OBJDIR'] on
            # mac, and that confuses mach.
            del env['MOZ_OBJDIR']
            return self.get_output_from_command(
                cmd, cwd=dirs['abs_obj_dir'], env=env)
        else:
            return None

    def query_buildid(self):
        if self.buildid:
            return self.buildid

        # for taskcluster, we pass MOZ_BUILD_DATE into mozharness as an
        # environment variable, only to have it pass the same value out with
        # the same name.
        buildid = os.environ.get('MOZ_BUILD_DATE')

        if not buildid:
            self.info("Creating buildid through current time")
            buildid = generate_build_ID()

        self.buildid = buildid
        return self.buildid

    def _query_objdir(self):
        if self.objdir:
            return self.objdir

        if not self.config.get('objdir'):
            return self.fatal(MISSING_CFG_KEY_MSG % ('objdir',))
        self.objdir = self.config['objdir']
        return self.objdir

    def query_is_nightly_promotion(self):
        platform_enabled = self.config.get('enable_nightly_promotion')
        branch_enabled = self.branch in self.config.get('nightly_promotion_branches')
        return platform_enabled and branch_enabled

    def query_build_env(self, **kwargs):
        c = self.config

        # let's evoke the base query_env and make a copy of it
        # as we don't always want every key below added to the same dict
        env = copy.deepcopy(
            super(BuildScript, self).query_env(**kwargs)
        )

        # first grab the buildid
        env['MOZ_BUILD_DATE'] = self.query_buildid()

        if self.query_is_nightly() or self.query_is_nightly_promotion():
            # taskcluster sets the update channel for shipping builds
            # explicitly
            if c.get('update_channel'):
                update_channel = c['update_channel']
                if isinstance(update_channel, unicode):
                    update_channel = update_channel.encode("utf-8")
                env["MOZ_UPDATE_CHANNEL"] = update_channel
            else:  # let's just give the generic channel based on branch
                env["MOZ_UPDATE_CHANNEL"] = "nightly-%s" % (self.branch,)
            self.info("Update channel set to: {}".format(env["MOZ_UPDATE_CHANNEL"]))

        if self.config.get('pgo_build') or self._compile_against_pgo():
            env['MOZ_PGO'] = '1'

        return env

    def query_mach_build_env(self, multiLocale=None):
        c = self.config
        if multiLocale is None and self.query_is_nightly():
            multiLocale = c.get('multi_locale', False)
        mach_env = {}
        if c.get('upload_env'):
            mach_env.update(c['upload_env'])

        # this prevents taskcluster from overwriting the target files with
        # the multilocale files. Put everything from the en-US build in a
        # separate folder.
        if multiLocale and self.config.get('taskcluster_nightly'):
            if 'UPLOAD_PATH' in mach_env:
                mach_env['UPLOAD_PATH'] = os.path.join(mach_env['UPLOAD_PATH'],
                                                       'en-US')
        return mach_env

    def _compile_against_pgo(self):
        """determines whether a build should be run with pgo even if it is
        not a classified as a 'pgo build'.

        requirements:
        1) must be a platform that can run against pgo
        2) must be a nightly build
        """
        c = self.config
        if self.stage_platform in c['pgo_platforms']:
            if self.query_is_nightly():
                return True
        return False

    def query_check_test_env(self):
        c = self.config
        dirs = self.query_abs_dirs()
        check_test_env = {}
        if c.get('check_test_env'):
            for env_var, env_value in c['check_test_env'].iteritems():
                check_test_env[env_var] = env_value % dirs
        # Check tests don't upload anything, however our mozconfigs depend on
        # UPLOAD_PATH, so we prevent configure from re-running by keeping the
        # environments consistent.
        if c.get('upload_env'):
            check_test_env.update(c['upload_env'])
        return check_test_env

    def _rm_old_package(self):
        """rm the old package."""
        c = self.config
        dirs = self.query_abs_dirs()
        old_package_paths = []
        old_package_patterns = c.get('old_packages')

        self.info("removing old packages...")
        if os.path.exists(dirs['abs_obj_dir']):
            for product in old_package_patterns:
                old_package_paths.extend(
                    glob.glob(product % {"objdir": dirs['abs_obj_dir']})
                )
        if old_package_paths:
            for package_path in old_package_paths:
                self.rmtree(package_path)
        else:
            self.info("There wasn't any old packages to remove.")

    def _get_mozconfig(self):
        """assign mozconfig."""
        dirs = self.query_abs_dirs()

        try:
            abs_mozconfig_path = get_mozconfig_path(
                script=self, config=self.config, dirs=dirs)
        except MozconfigPathError as e:
            self.fatal(e.message)

        self.info("Use mozconfig: {}".format(abs_mozconfig_path))

        # print its contents
        content = self.read_from_file(abs_mozconfig_path, error_level=FATAL)
        self.info("mozconfig content:")
        self.info(content)

        # finally, copy the mozconfig to a path that 'mach build' expects it to be
        self.copyfile(abs_mozconfig_path, os.path.join(dirs['abs_src_dir'], '.mozconfig'))

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
        env = self.query_build_env()
        env.update(self.query_mach_build_env())

        self._assert_cfg_valid_for_action(
            ['tooltool_url'],
            'build'
        )
        c = self.config
        dirs = self.query_abs_dirs()
        toolchains = os.environ.get('MOZ_TOOLCHAINS')
        manifest_src = os.environ.get('TOOLTOOL_MANIFEST')
        if not manifest_src:
            manifest_src = c.get('tooltool_manifest_src')
        if not manifest_src and not toolchains:
            return self.warning(ERROR_MSGS['tooltool_manifest_undetermined'])
        cmd = [
            sys.executable, '-u',
            os.path.join(dirs['abs_src_dir'], 'mach'),
            'artifact',
            'toolchain',
            '-v',
            '--retry', '4',
            '--artifact-manifest',
            os.path.join(dirs['abs_src_dir'], 'toolchains.json'),
        ]
        if manifest_src:
            cmd.extend([
                '--tooltool-manifest',
                os.path.join(dirs['abs_src_dir'], manifest_src),
                '--tooltool-url',
                c['tooltool_url'],
            ])
            auth_file = self._get_tooltool_auth_file()
            if auth_file:
                cmd.extend(['--authentication-file', auth_file])
        cache = c['env'].get('TOOLTOOL_CACHE')
        if cache:
            cmd.extend(['--cache-dir', cache])
        if toolchains:
            cmd.extend(toolchains.split())
        self.info(str(cmd))
        self.run_command(cmd, cwd=dirs['abs_src_dir'], halt_on_failure=True,
                         env=env)

    def generate_build_props(self, console_output=True, halt_on_failure=False):
        """sets props found from mach build and, in addition, buildid,
        sourcestamp,  appVersion, and appName."""

        error_level = ERROR
        if halt_on_failure:
            error_level = FATAL

        if self.generated_build_props:
            return

        dirs = self.query_abs_dirs()
        print_conf_setting_path = os.path.join(dirs['abs_src_dir'],
                                               'config',
                                               'printconfigsetting.py')
        if (not os.path.exists(print_conf_setting_path) or
                not os.path.exists(dirs['abs_app_ini_path'])):
            self.log("Can't set the following properties: "
                     "buildid, sourcestamp, appVersion, and appName. "
                     "Required paths missing. Verify both %s and %s "
                     "exist. These paths require the 'build' action to be "
                     "run prior to this" % (print_conf_setting_path,
                                            dirs['abs_app_ini_path']),
                     level=error_level)
        self.info("Setting properties found in: %s" % dirs['abs_app_ini_path'])
        env = self.query_build_env()
        # dirs['abs_obj_dir'] can be different from env['MOZ_OBJDIR'] on
        # mac, and that confuses mach.
        del env['MOZ_OBJDIR']

        if self.config.get('is_automation'):
            self.info("Verifying buildid from application.ini matches buildid "
                      "from automation")
            app_ini_buildid = self._query_build_prop_from_app_ini('BuildID')
            # it would be hard to imagine query_buildid evaluating to a falsey
            #  value (e.g. 0), but incase it does, force it to None
            automation_buildid = self.query_buildid() or None
            self.info(
                'buildid from application.ini: "%s". buildid from automation '
                'properties: "%s"' % (app_ini_buildid, automation_buildid)
            )
            if app_ini_buildid == automation_buildid is not None:
                self.info('buildids match.')
            else:
                self.error(
                    'buildids do not match or values could not be determined'
                )
                # set the build to orange if not already worse
                self.return_code = self.worst_level(
                    EXIT_STATUS_DICT[TBPL_WARNING], self.return_code,
                    AUTOMATION_EXIT_CODES[::-1]
                )

        self.generated_build_props = True

    def _create_mozbuild_dir(self, mozbuild_path=None):
        if not mozbuild_path:
            env = self.query_build_env()
            mozbuild_path = env.get('MOZBUILD_STATE_PATH')
        if mozbuild_path:
            self.mkdir_p(mozbuild_path)
        else:
            self.warning("mozbuild_path could not be determined. skipping "
                         "creating it.")

    def preflight_build(self):
        """set up machine state for a complete build."""
        if not self.query_is_nightly():
            # the old package should live in source dir so we don't need to do
            # this for nighties since we clobber the whole work_dir in
            # clobber()
            self._rm_old_package()
        self._get_mozconfig()
        self._run_tooltool()
        self._create_mozbuild_dir()
        self._ensure_upload_path()
        mach_props = os.path.join(
            self.query_abs_dirs()['abs_obj_dir'], 'dist', 'mach_build_properties.json'
        )
        if os.path.exists(mach_props):
            self.info("Removing previous mach property file: %s" % mach_props)
            self.rmtree(mach_props)

    def build(self):
        """builds application."""

        args = ['build', '-v']

        custom_build_targets = self.config.get('build_targets')
        if custom_build_targets:
            args += custom_build_targets

        # This will error on non-0 exit code.
        self._run_mach_command_in_build_env(args)

        if not custom_build_targets:
            self.generate_build_props(console_output=True, halt_on_failure=True)

        self._generate_build_stats()

    def static_analysis_autotest(self):
        """Run mach static-analysis autotest, in order to make sure we dont regress"""
        self.preflight_build()
        self._run_mach_command_in_build_env(['configure'])
        self._run_mach_command_in_build_env(['static-analysis', 'autotest',
                                             '--intree-tool'],
                                            use_subprocess=True)

    def _query_mach(self):
        dirs = self.query_abs_dirs()

        if 'MOZILLABUILD' in os.environ:
            # We found many issues with intermittent build failures when not
            # invoking mach via bash.
            # See bug 1364651 before considering changing.
            mach = [
                os.path.join(os.environ['MOZILLABUILD'], 'msys', 'bin', 'bash.exe'),
                os.path.join(dirs['abs_src_dir'], 'mach')
            ]
        else:
            mach = [sys.executable, 'mach']
        return mach

    def _run_mach_command_in_build_env(self, args, use_subprocess=False):
        """Run a mach command in a build context."""
        env = self.query_build_env()
        env.update(self.query_mach_build_env())

        dirs = self.query_abs_dirs()

        mach = self._query_mach()

        # XXX See bug 1483883
        # Work around an interaction between Gradle and mozharness
        # Not using `subprocess` causes gradle to hang
        if use_subprocess:
            import subprocess
            return_code = subprocess.call(mach + ['--log-no-times'] + args,
                                          env=env, cwd=dirs['abs_src_dir'])
        else:
            return_code = self.run_command(
                command=mach + ['--log-no-times'] + args,
                cwd=dirs['abs_src_dir'],
                env=env,
                error_list=MakefileErrorList,
                output_timeout=self.config.get('max_build_output_timeout',
                                               60 * 40)
            )

        if return_code:
            self.return_code = self.worst_level(
                EXIT_STATUS_DICT[TBPL_FAILURE], self.return_code,
                AUTOMATION_EXIT_CODES[::-1]
            )
            self.fatal("'mach %s' did not run successfully. Please check "
                       "log for errors." % ' '.join(args))

    def multi_l10n(self):
        if not self.query_is_nightly():
            self.info("Not a nightly build, skipping multi l10n.")
            return

        dirs = self.query_abs_dirs()
        base_work_dir = dirs['base_work_dir']
        objdir = dirs['abs_obj_dir']
        branch = self.branch

        # Building a nightly with the try repository fails because a
        # config-file does not exist for try. Default to mozilla-central
        # settings (arbitrarily).
        if branch == 'try':
            branch = 'mozilla-central'

        multi_config_pf = self.config.get('multi_locale_config_platform',
                                          'android')

        multil10n_path = 'build/src/testing/mozharness/scripts/multil10n.py'
        base_work_dir = os.path.join(base_work_dir, 'workspace')

        cmd = [
            sys.executable,
            multil10n_path,
            '--config-file',
            'multi_locale/%s_%s.json' % (branch, multi_config_pf),
            '--config-file',
            'multi_locale/android-mozharness-build.json',
            '--pull-locale-source',
            '--package-multi',
            '--summary',
        ]

        self.run_command(cmd, env=self.query_build_env(), cwd=base_work_dir,
                         halt_on_failure=True)

        package_cmd = [
            'make',
            'echo-variable-PACKAGE',
            'AB_CD=multi',
        ]
        package_filename = self.get_output_from_command(
            package_cmd,
            cwd=objdir,
        )
        if not package_filename:
            self.fatal(
                "Unable to determine the package filename for the multi-l10n build. "
                "Was trying to run: %s" % package_cmd)

        self.info('Multi-l10n package filename is: %s' % package_filename)

        parser = MakeUploadOutputParser(config=self.config,
                                        log_obj=self.log_obj,
                                        )
        upload_cmd = ['make', 'upload', 'AB_CD=multi']
        self.run_command(upload_cmd,
                         env=self.query_mach_build_env(multiLocale=False),
                         cwd=objdir, halt_on_failure=True,
                         output_parser=parser)
        upload_files_cmd = [
            'make',
            'echo-variable-UPLOAD_FILES',
            'AB_CD=multi',
        ]
        self.get_output_from_command(
            upload_files_cmd,
            cwd=objdir,
        )

    def postflight_build(self):
        """grabs properties from post build and calls ccache -s"""
        # A list of argument lists.  Better names gratefully accepted!
        mach_commands = self.config.get('postflight_build_mach_commands', [])
        for mach_command in mach_commands:
            self._execute_postflight_build_mach_command(mach_command)

    def _execute_postflight_build_mach_command(self, mach_command_args):
        env = self.query_build_env()
        env.update(self.query_mach_build_env())

        command = [sys.executable, 'mach', '--log-no-times']
        command.extend(mach_command_args)

        self.run_command(
            command=command,
            cwd=self.query_abs_dirs()['abs_src_dir'],
            env=env, output_timeout=self.config.get('max_build_output_timeout', 60 * 20),
            halt_on_failure=True,
        )

    def preflight_package_source(self):
        self._get_mozconfig()

    def package_source(self):
        """generates source archives and uploads them"""
        env = self.query_build_env()
        env.update(self.query_mach_build_env())
        dirs = self.query_abs_dirs()

        self.run_command(
            command=[sys.executable, 'mach', '--log-no-times', 'configure'],
            cwd=dirs['abs_src_dir'],
            env=env, output_timeout=60*3, halt_on_failure=True,
        )
        self.run_command(
            command=[
                'make', 'source-package', 'source-upload',
            ],
            cwd=dirs['abs_obj_dir'],
            env=env, output_timeout=60*45, halt_on_failure=True,
        )

    def check_test(self):
        if os.environ.get('USE_ARTIFACT'):
            self.info('Skipping due to forced artifact build.')
            return
        c = self.config
        dirs = self.query_abs_dirs()

        env = self.query_build_env()
        env.update(self.query_check_test_env())

        cmd = self._query_mach() + [
            '--log-no-times',
            'build',
            '-v',
            '--keep-going',
            'check',
        ]

        parser = CheckTestCompleteParser(config=c,
                                         log_obj=self.log_obj)
        return_code = self.run_command(command=cmd,
                                       cwd=dirs['abs_src_dir'],
                                       env=env,
                                       output_parser=parser)
        tbpl_status = parser.evaluate_parser(return_code)
        return_code = EXIT_STATUS_DICT[tbpl_status]

        if return_code:
            self.return_code = self.worst_level(
                return_code,  self.return_code,
                AUTOMATION_EXIT_CODES[::-1]
            )
            self.error("'mach build check' did not run successfully. Please "
                       "check log for errors.")

    def _is_configuration_shipped(self):
        """Determine if the current build configuration is shipped to users.

        This is used to drive alerting so we don't see alerts for build
        configurations we care less about.
        """
        # Ideally this would be driven by a config option. However, our
        # current inheritance mechanism of using a base config and then
        # one-off configs for variants isn't conducive to this since derived
        # configs we need to be reset and we don't like requiring boilerplate
        # in derived configs.

        # All PGO builds are shipped. This takes care of Linux and Windows.
        if self.config.get('pgo_build'):
            return True

        # Debug builds are never shipped.
        if self.config.get('debug_build'):
            return False

        # OS X opt builds without a variant are shipped.
        if self.config.get('platform') == 'macosx64':
            if not self.config.get('build_variant'):
                return True

        # Android opt builds without a variant are shipped.
        if self.config.get('platform') == 'android':
            if not self.config.get('build_variant'):
                return True

        return False

    def _load_build_resources(self):
        p = self.config.get('build_resources_path') % self.query_abs_dirs()
        if not os.path.exists(p):
            self.info('%s does not exist; not loading build resources' % p)
            return None

        with open(p, 'rb') as fh:
            resources = json.load(fh)

        if 'duration' not in resources:
            self.info('resource usage lacks duration; ignoring')
            return None

        data = {
            'name': 'build times',
            'value': resources['duration'],
            'extraOptions': self.perfherder_resource_options(),
            'subtests': [],
        }

        for phase in resources['phases']:
            if 'duration' not in phase:
                continue
            data['subtests'].append({
                'name': phase['name'],
                'value': phase['duration'],
            })

        return data

    def _load_sccache_stats(self):
        stats_file = os.path.join(
            self.query_abs_dirs()['abs_obj_dir'], 'sccache-stats.json'
        )
        if not os.path.exists(stats_file):
            self.info('%s does not exist; not loading sccache stats' % stats_file)
            return

        with open(stats_file, 'rb') as fh:
            stats = json.load(fh)

        def get_stat(key):
            val = stats['stats'][key]
            # Future versions of sccache will distinguish stats by language
            # and store them as a dict.
            if isinstance(val, dict):
                val = sum(val['counts'].values())
            return val

        total = get_stat('requests_executed')
        hits = get_stat('cache_hits')
        if total > 0:
            hits /= float(total)

        yield {
            'name': 'sccache hit rate',
            'value': hits,
            'extraOptions': self.perfherder_resource_options(),
            'subtests': [],
            'lowerIsBetter': False
        }

        yield {
            'name': 'sccache cache_write_errors',
            'value': stats['stats']['cache_write_errors'],
            'extraOptions': self.perfherder_resource_options(),
            'alertThreshold': 50.0,
            'subtests': [],
        }

        yield {
            'name': 'sccache requests_not_cacheable',
            'value': stats['stats']['requests_not_cacheable'],
            'extraOptions': self.perfherder_resource_options(),
            'alertThreshold': 50.0,
            'subtests': [],
        }

    def _get_package_metrics(self):
        import tarfile
        import zipfile

        dirs = self.query_abs_dirs()

        dist_dir = os.path.join(dirs['abs_obj_dir'], 'dist')
        for ext in ['apk', 'dmg', 'tar.bz2', 'zip']:
            name = 'target.' + ext
            if os.path.exists(os.path.join(dist_dir, name)):
                packageName = name
                break
        else:
            self.fatal("could not determine packageName")

        interests = ['libxul.so', 'classes.dex', 'omni.ja', 'xul.dll']
        installer = os.path.join(dist_dir, packageName)
        installer_size = 0
        size_measurements = []

        def paths_with_sizes(installer):
            if zipfile.is_zipfile(installer):
                with zipfile.ZipFile(installer, 'r') as zf:
                    for zi in zf.infolist():
                        yield zi.filename, zi.file_size
            elif tarfile.is_tarfile(installer):
                with tarfile.open(installer, 'r:*') as tf:
                    for ti in tf:
                        yield ti.name, ti.size

        if os.path.exists(installer):
            installer_size = self.query_filesize(installer)
            self.info('Size of %s: %s bytes' % (packageName, installer_size))
            try:
                subtests = {}
                for path, size in paths_with_sizes(installer):
                    name = os.path.basename(path)
                    if name in interests:
                        # We have to be careful here: desktop Firefox installers
                        # contain two omni.ja files: one for the general runtime,
                        # and one for the browser proper.
                        if name == 'omni.ja':
                            containing_dir = os.path.basename(os.path.dirname(path))
                            if containing_dir == 'browser':
                                name = 'browser-omni.ja'
                        if name in subtests:
                            self.fatal('should not see %s (%s) multiple times!'
                                       % (name, path))
                        subtests[name] = size
                for name in subtests:
                    self.info('Size of %s: %s bytes' % (name,
                                                        subtests[name]))
                    size_measurements.append(
                        {'name': name, 'value': subtests[name]})
            except Exception:
                self.info('Unable to search %s for component sizes.' % installer)
                size_measurements = []

        if not installer_size and not size_measurements:
            return

        # We want to always collect metrics. But alerts for installer size are
        # only use for builds with ship. So nix the alerts for builds we don't
        # ship.
        def filter_alert(alert):
            if not self._is_configuration_shipped():
                alert['shouldAlert'] = False

            return alert

        if installer.endswith('.apk'):  # Android
            yield filter_alert({
                "name": "installer size",
                "value": installer_size,
                "alertChangeType": "absolute",
                "alertThreshold": (200 * 1024),
                "subtests": size_measurements
            })
        else:
            yield filter_alert({
                "name": "installer size",
                "value": installer_size,
                "alertChangeType": "absolute",
                "alertThreshold": (100 * 1024),
                "subtests": size_measurements
            })

    def _get_sections(self, file, filter=None):
        """
        Returns a dictionary of sections and their sizes.
        """
        # Check for `rust_size`, our cross platform version of size. It should
        # be installed by tooltool in $abs_src_dir/rust-size/rust-size
        rust_size = os.path.join(self.query_abs_dirs()['abs_src_dir'],
                                 'rust-size', 'rust-size')
        size_prog = self.which(rust_size)
        if not size_prog:
            self.info("Couldn't find `rust-size` program")
            return {}

        self.info("Using %s" % size_prog)
        cmd = [size_prog, file]
        output = self.get_output_from_command(cmd)
        if not output:
            self.info("`rust-size` failed")
            return {}

        # Format is JSON:
        # {
        #   "section_type": {
        #     "section_name": size, ....
        #   },
        #   ...
        # }
        try:
            parsed = json.loads(output)
        except ValueError:
            self.info("`rust-size` failed: %s" % output)
            return {}

        sections = {}
        for sec_type in parsed.itervalues():
            for name, size in sec_type.iteritems():
                if not filter or name in filter:
                    sections[name] = size

        return sections

    def _get_binary_metrics(self):
        """
        Provides metrics on interesting compenents of the built binaries.
        Currently just the sizes of interesting sections.
        """
        lib_interests = {
            'XUL': ('libxul.so', 'xul.dll', 'XUL'),
            'NSS': ('libnss3.so', 'nss3.dll', 'libnss3.dylib'),
            'NSPR': ('libnspr4.so', 'nspr4.dll', 'libnspr4.dylib'),
            'avcodec': ('libmozavcodec.so', 'mozavcodec.dll', 'libmozavcodec.dylib'),
            'avutil': ('libmozavutil.so', 'mozavutil.dll', 'libmozavutil.dylib')
        }
        section_interests = ('.text', '.data', '.rodata', '.rdata',
                             '.cstring', '.data.rel.ro', '.bss')
        lib_details = []

        dirs = self.query_abs_dirs()
        dist_dir = os.path.join(dirs['abs_obj_dir'], 'dist')
        bin_dir = os.path.join(dist_dir, 'bin')

        for lib_type, lib_names in lib_interests.iteritems():
            for lib_name in lib_names:
                lib = os.path.join(bin_dir, lib_name)
                if os.path.exists(lib):
                    lib_size = 0
                    section_details = self._get_sections(lib, section_interests)
                    section_measurements = []
                    # Build up the subtests

                    # Lump rodata sections together
                    # - Mach-O separates out read-only string data as .cstring
                    # - PE really uses .rdata, but XUL at least has a .rodata as well
                    for ro_alias in ('.cstring', '.rdata'):
                        if ro_alias in section_details:
                            if '.rodata' in section_details:
                                section_details['.rodata'] += section_details[ro_alias]
                            else:
                                section_details['.rodata'] = section_details[ro_alias]
                            del section_details[ro_alias]

                    for k, v in section_details.iteritems():
                        section_measurements.append({'name': k, 'value': v})
                        lib_size += v
                    lib_details.append({
                        'name': lib_type,
                        'size': lib_size,
                        'sections': section_measurements
                    })

        for lib_detail in lib_details:
            yield {
                "name": "%s section sizes" % lib_detail['name'],
                "value": lib_detail['size'],
                "shouldAlert": False,
                "subtests": lib_detail['sections']
            }

    def _generate_build_stats(self):
        """grab build stats following a compile.

        This action handles all statistics from a build: 'count_ctors'
        and then posts to graph server the results.
        We only post to graph server for non nightly build
        """
        self.info('Collecting build metrics')

        if os.environ.get('USE_ARTIFACT'):
            self.info('Skipping due to forced artifact build.')
            return

        c = self.config

        # Report some important file sizes for display in treeherder

        perfherder_data = {
            "framework": {
                "name": "build_metrics"
            },
            "suites": [],
        }

        if not c.get('debug_build') and not c.get('disable_package_metrics'):
            perfherder_data['suites'].extend(self._get_package_metrics())
            perfherder_data['suites'].extend(self._get_binary_metrics())

        # Extract compiler warnings count.
        warnings = self.get_output_from_command(
            command=[sys.executable, 'mach', 'warnings-list'],
            cwd=self.query_abs_dirs()['abs_src_dir'],
            env=self.query_build_env(),
            # No need to pollute the log.
            silent=True,
            # Fail fast.
            halt_on_failure=True)

        if warnings is not None:
            perfherder_data['suites'].append({
                'name': 'compiler warnings',
                'value': len(warnings.strip().splitlines()),
                'alertThreshold': 100.0,
                'subtests': [],
            })

        build_metrics = self._load_build_resources()
        if build_metrics:
            perfherder_data['suites'].append(build_metrics)
        perfherder_data['suites'].extend(self._load_sccache_stats())

        # Ensure all extra options for this configuration are present.
        for opt in os.environ.get('PERFHERDER_EXTRA_OPTIONS', '').split():
            for suite in perfherder_data['suites']:
                if opt not in suite.get('extraOptions', []):
                    suite.setdefault('extraOptions', []).append(opt)

        if self.query_is_nightly():
            for suite in perfherder_data['suites']:
                suite.setdefault('extraOptions', []).insert(0, 'nightly')

        if perfherder_data["suites"]:
            self.info('PERFHERDER_DATA: %s' % json.dumps(perfherder_data))

    def valgrind_test(self):
        '''Execute mach's valgrind-test for memory leaks'''
        env = self.query_build_env()
        env.update(self.query_mach_build_env())

        return_code = self.run_command(
            command=[sys.executable, 'mach', 'valgrind-test'],
            cwd=self.query_abs_dirs()['abs_src_dir'],
            env=env, output_timeout=self.config.get('max_build_output_timeout', 60 * 40)
        )
        if return_code:
            self.return_code = self.worst_level(
                EXIT_STATUS_DICT[TBPL_FAILURE],  self.return_code,
                AUTOMATION_EXIT_CODES[::-1]
            )
            self.fatal("'mach valgrind-test' did not run successfully. Please check "
                       "log for errors.")

    def _ensure_upload_path(self):
        env = self.query_mach_build_env()

        # Some Taskcluster workers don't like it if an artifacts directory
        # is defined but no artifacts are uploaded. Guard against this by always
        # ensuring the artifacts directory exists.
        if 'UPLOAD_PATH' in env and not os.path.exists(env['UPLOAD_PATH']):
            self.mkdir_p(env['UPLOAD_PATH'])

    def _post_fatal(self, message=None, exit_code=None):
        if not self.return_code:  # only overwrite return_code if it's 0
            self.error('setting return code to 2 because fatal was called')
            self.return_code = 2

    @PostScriptRun
    def _shutdown_sccache(self):
        '''If sccache was in use for this build, shut down the sccache server.'''
        if os.environ.get('USE_SCCACHE') == '1':
            topsrcdir = self.query_abs_dirs()['abs_src_dir']
            sccache = os.path.join(topsrcdir, 'sccache2', 'sccache')
            if self._is_windows():
                sccache += '.exe'
            self.run_command([sccache, '--stop-server'], cwd=topsrcdir)

    @PostScriptRun
    def _summarize(self):
        """ If this is run in automation, ensure the return code is valid and
        set it to one if it's not. Finally, log any summaries we collected
        from the script run.
        """
        if self.config.get("is_automation"):
            # let's ignore all mention of tbpl status until this
            # point so it will be easier to manage
            if self.return_code not in AUTOMATION_EXIT_CODES:
                self.error("Return code is set to: %s and is outside of "
                           "automation's known values. Setting to 2(failure). "
                           "Valid return codes %s" % (self.return_code,
                                                      AUTOMATION_EXIT_CODES))
                self.return_code = 2
            for status, return_code in EXIT_STATUS_DICT.iteritems():
                if return_code == self.return_code:
                    self.record_status(status, TBPL_STATUS_DICT[status])
        self.summary()

    @PostScriptRun
    def _parse_build_tests_ccov(self):
        if 'MOZ_FETCHES_DIR' not in os.environ:
            return

        dirs = self.query_abs_dirs()
        topsrcdir = dirs['abs_src_dir']
        base_work_dir = dirs['base_work_dir']

        env = self.query_build_env()

        grcov_path = os.path.join(os.environ['MOZ_FETCHES_DIR'], 'grcov')
        if not os.path.isabs(grcov_path):
            grcov_path = os.path.join(base_work_dir, grcov_path)
        if self._is_windows():
            grcov_path += '.exe'
        env['GRCOV_PATH'] = grcov_path

        cmd = self._query_mach() + [
            'python',
            os.path.join('testing', 'parse_build_tests_ccov.py'),
        ]
        self.run_command(command=cmd, cwd=topsrcdir, env=env, halt_on_failure=True)
