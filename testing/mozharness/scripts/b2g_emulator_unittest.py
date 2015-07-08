#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import os
import re
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import BaseErrorList, TarErrorList
from mozharness.base.log import ERROR, WARNING
from mozharness.base.script import (
    BaseScript,
    PreScriptAction,
)
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.testing.errors import LogcatErrorList
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.buildbot import TBPL_SUCCESS


class B2GEmulatorTest(TestingMixin, VCSMixin, BaseScript, BlobUploadMixin):
    test_suites = ('jsreftest', 'reftest', 'mochitest', 'mochitest-chrome', 'xpcshell', 'crashtest', 'cppunittest')
    config_options = [[
        ["--type"],
        {"action": "store",
         "dest": "test_type",
         "default": "browser",
         "help": "The type of tests to run",
         }
    ], [
        ["--busybox-url"],
        {"action": "store",
         "dest": "busybox_url",
         "default": None,
         "help": "URL to the busybox binary",
         }
    ], [
        ["--emulator-url"],
        {"action": "store",
         "dest": "emulator_url",
         "default": None,
         "help": "URL to the emulator zip",
         }
    ], [
        ["--xre-url"],
        {"action": "store",
         "dest": "xre_url",
         "default": None,
         "help": "URL to the desktop xre zip",
         }
    ], [
        ["--gecko-url"],
        {"action": "store",
         "dest": "gecko_url",
         "default": None,
         "help": "URL to the gecko build injected into the emulator",
         }
    ], [
        ["--test-manifest"],
        {"action": "store",
         "dest": "test_manifest",
         "default": None,
         "help": "Path to test manifest to run",
         }
    ], [
        ["--test-suite"],
        {"action": "store",
         "dest": "test_suite",
         "type": "choice",
         "choices": test_suites,
         "help": "Which test suite to run",
         }
    ], [
        ["--adb-path"],
        {"action": "store",
         "dest": "adb_path",
         "default": None,
         "help": "Path to adb",
         }
    ], [
        ["--total-chunks"],
        {"action": "store",
         "dest": "total_chunks",
         "help": "Number of total chunks",
         }
    ], [
        ["--this-chunk"],
        {"action": "store",
         "dest": "this_chunk",
         "help": "Number of this chunk",
         }
    ], [
        ["--test-path"],
        {"action": "store",
         "dest": "test_path",
         "help": "Path of tests to run",
         }
    ]] + copy.deepcopy(testing_config_options) \
       + copy.deepcopy(blobupload_config_options)

    error_list = [
        {'substr': 'FAILED (errors=', 'level': ERROR},
        {'substr': r'''Could not successfully complete transport of message to Gecko, socket closed''', 'level': ERROR},
        {'substr': r'''Could not communicate with Marionette server. Is the Gecko process still running''', 'level': ERROR},
        {'substr': r'''Connection to Marionette server is lost. Check gecko''', 'level': ERROR},
        {'substr': 'Timeout waiting for marionette on port', 'level': ERROR},
        {'regex': re.compile(r'''(Timeout|NoSuchAttribute|Javascript|NoSuchElement|XPathLookup|NoSuchWindow|StaleElement|ScriptTimeout|ElementNotVisible|NoSuchFrame|InvalidElementState|NoAlertPresent|InvalidCookieDomain|UnableToSetCookie|InvalidSelector|MoveTargetOutOfBounds)Exception'''), 'level': ERROR},
    ]

    def __init__(self, require_config_file=False):
        super(B2GEmulatorTest, self).__init__(
            config_options=self.config_options,
            all_actions=['clobber',
                         'read-buildbot-config',
                         'download-and-extract',
                         'create-virtualenv',
                         'install',
                         'run-tests'],
            default_actions=['clobber',
                             'download-and-extract',
                             'create-virtualenv',
                             'install',
                             'run-tests'],
            require_config_file=require_config_file,
            config={
                'require_test_zip': True,
                # This is a special IP that has meaning to the emulator
                'remote_webserver': '10.0.2.2',
            }
        )

        # these are necessary since self.config is read only
        c = self.config
        self.adb_path = c.get('adb_path', self._query_adb())
        self.installer_url = c.get('installer_url')
        self.installer_path = c.get('installer_path')
        self.test_url = c.get('test_url')
        self.test_manifest = c.get('test_manifest')
        self.busybox_path = None

    # TODO detect required config items and fail if not set

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(B2GEmulatorTest, self).query_abs_dirs()
        dirs = {}
        dirs['abs_test_install_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_xre_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'xre')
        dirs['abs_emulator_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'emulator')
        dirs['abs_blob_upload_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        dirs['abs_b2g-distro_dir'] = os.path.join(
            dirs['abs_emulator_dir'], 'b2g-distro')
        dirs['abs_mochitest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'mochitest')
        dirs['abs_mochitest-chrome_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'mochitest')
        dirs['abs_certs_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'certs')
        dirs['abs_modules_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'modules')
        dirs['abs_reftest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'reftest')
        dirs['abs_crashtest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'reftest')
        dirs['abs_jsreftest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'reftest')
        dirs['abs_xpcshell_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'xpcshell')
        dirs['abs_cppunittest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'cppunittest')
        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def download_and_extract(self):
        target_suite = self.config['test_suite']
        super(B2GEmulatorTest, self).download_and_extract(suite_categories=[target_suite])
        dirs = self.query_abs_dirs()

        self.mkdir_p(dirs['abs_emulator_dir'])
        tar = self.query_exe('tar', return_type='list')
        self.run_command(tar + ['zxf', self.installer_path],
                         cwd=dirs['abs_emulator_dir'],
                         error_list=TarErrorList,
                         halt_on_failure=True, fatal_exit_code=3)

        self.mkdir_p(dirs['abs_xre_dir'])
        self._download_unzip(self.config['xre_url'],
                             dirs['abs_xre_dir'])

        if self.config.get('busybox_url'):
            self.download_file(self.config['busybox_url'],
                               file_name='busybox',
                               parent_dir=dirs['abs_work_dir'])
            self.busybox_path = os.path.join(dirs['abs_work_dir'], 'busybox')

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        if self.tree_config.get('use_puppetagain_packages'):
            requirements = [os.path.join('tests', 'b2g',
                            'b2g-unittest-requirements.txt')]

            self.register_virtualenv_module(
                'mozinstall',
                requirements=requirements
            )
            self.register_virtualenv_module(
                'marionette',
                url=os.path.join('tests', 'marionette'),
                requirements=requirements
            )
            return

        dirs = self.query_abs_dirs()
        requirements = os.path.join(dirs['abs_test_install_dir'],
                                    'config',
                                    'marionette_requirements.txt')
        if os.path.isfile(requirements):
            self.register_virtualenv_module(requirements=[requirements],
                                            two_pass=True)
            return

        # XXX Bug 879765: Dependent modules need to be listed before parent
        # modules, otherwise they will get installed from the pypi server.
        # XXX Bug 908356: This block can be removed as soon as the
        # in-tree requirements files propagate to all active trees.
        mozbase_dir = os.path.join('tests', 'mozbase')
        self.register_virtualenv_module(
            'manifestparser',
            url=os.path.join(mozbase_dir, 'manifestdestiny')
        )

        for m in ('mozfile', 'mozlog', 'mozinfo', 'moznetwork', 'mozhttpd',
                  'mozcrash', 'mozinstall', 'mozdevice', 'mozprofile', 'mozprocess',
                  'mozrunner'):
            self.register_virtualenv_module(
                m, url=os.path.join(mozbase_dir, m))

        self.register_virtualenv_module(
            'marionette', url=os.path.join('tests', 'marionette'))

    def _query_abs_base_cmd(self, suite):
        dirs = self.query_abs_dirs()
        cmd = [self.query_python_path('python')]
        cmd.append(self.config['run_file_names'][suite])

        raw_log_file = os.path.join(dirs['abs_blob_upload_dir'],
                                    '%s_raw.log' % suite)
        emulator_type = 'x86' if os.path.isdir(os.path.join(dirs['abs_b2g-distro_dir'],
                        'out', 'target', 'product', 'generic_x86')) else 'arm'
        self.info("The emulator type: %s" % emulator_type)

        str_format_values = {
            'adbpath': self.adb_path,
            'b2gpath': dirs['abs_b2g-distro_dir'],
            'emulator': emulator_type,
            'logcat_dir': dirs['abs_work_dir'],
            'modules_dir': dirs['abs_modules_dir'],
            'remote_webserver': self.config['remote_webserver'],
            'xre_path': os.path.join(dirs['abs_xre_dir'], 'bin'),
            'symbols_path': self.symbols_path,
            'busybox': self.busybox_path,
            'total_chunks': self.config.get('total_chunks'),
            'this_chunk': self.config.get('this_chunk'),
            'test_path': self.config.get('test_path'),
            'certificate_path': dirs['abs_certs_dir'],
            'raw_log_file': raw_log_file,
        }

        missing_key = True
        if "suite_definitions" in self.tree_config: # new structure
            if suite in self.tree_config["suite_definitions"]:
                missing_key = False
            options = self.tree_config["suite_definitions"][suite]["options"]
        else:
            suite_options = '%s_options' % suite
            if suite_options in self.tree_config:
                missing_key = False
            options = self.tree_config[suite_options]

        if missing_key:
            self.fatal("Key '%s' not defined in the in-tree config! Please add it to '%s'." \
                       "See bug 981030 for more details." % (suite,
                       os.path.join('gecko', 'testing', self.config['in_tree_config'])))

        if options:
            for option in options:
                option = option % str_format_values
                if not option.endswith('None'):
                    cmd.append(option)
        return cmd

    def _query_adb(self):
        return self.which('adb') or \
            os.getenv('ADB_PATH') or \
            os.path.join(self.query_abs_dirs()['abs_b2g-distro_dir'],
                         'out', 'host', 'linux-x86', 'bin', 'adb')

    def preflight_run_tests(self):
        super(B2GEmulatorTest, self).preflight_run_tests()
        suite = self.config['test_suite']
        # set default test manifest by suite if none specified
        if not self.test_manifest:
            if suite == 'reftest':
                self.test_manifest = os.path.join('tests', 'layout',
                                                  'reftests', 'reftest.list')
            elif suite == 'xpcshell':
                self.test_manifest = os.path.join('tests', 'xpcshell_b2g.ini')
            elif suite == 'crashtest':
                self.test_manifest = os.path.join('tests', 'testing',
                                                  'crashtest', 'crashtests.list')
            elif suite == 'jsreftest':
                self.test_manifest = os.path.join('jsreftest', 'tests',
                                                  'jstests.list')

        if not os.path.isfile(self.adb_path):
            self.fatal("The adb binary '%s' is not a valid file!" % self.adb_path)

    def install(self):
        # The emulator was extracted during the download_and_extract step;
        # there's no separate binary to install.  We call pass to prevent
        # the base implementation from running here.
        pass

    def _get_success_codes(self, suite_name):
        success_codes = None
        if suite_name == 'xpcshell':
            # bug 773703
            success_codes = [0, 1]
        return success_codes

    def run_tests(self):
        """
        Run the tests
        """
        dirs = self.query_abs_dirs()

        error_list = self.error_list
        error_list.extend(BaseErrorList)

        suite = self.config['test_suite']
        if suite not in self.test_suites:
            self.fatal("Don't know how to run --test-suite '%s'!" % suite)

        cmd = self._query_abs_base_cmd(suite)
        cwd = dirs['abs_%s_dir' % suite]
        cmd = self.append_harness_extra_args(cmd)

        # TODO we probably have to move some of the code in
        # scripts/desktop_unittest.py and scripts/marionette.py to
        # mozharness.mozilla.testing.unittest so we can share it.
        # In the short term, I'm ok with some duplication of code if it
        # expedites things; please file bugs to merge if that happens.

        suite_name = [x for x in self.test_suites if x in self.config['test_suite']][0]
        if self.config.get('this_chunk'):
            suite = '%s-%s' % (suite_name, self.config['this_chunk'])
        else:
            suite = suite_name

        env = {}
        if self.query_minidump_stackwalk():
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path
        env['MOZ_UPLOAD_DIR'] = dirs['abs_blob_upload_dir']
        if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
            self.mkdir_p(env['MOZ_UPLOAD_DIR'])
        env = self.query_env(partial_env=env)

        success_codes = self._get_success_codes(suite_name)
        parser = self.get_test_output_parser(suite_name,
                                             config=self.config,
                                             log_obj=self.log_obj,
                                             error_list=error_list)
        return_code = self.run_command(cmd, cwd=cwd, env=env,
                                       output_timeout=1000,
                                       output_parser=parser,
                                       success_codes=success_codes)

        logcat = os.path.join(dirs['abs_work_dir'], 'emulator-5554.log')

        qemu = os.path.join(dirs['abs_work_dir'], 'qemu.log')
        if os.path.isfile(qemu):
            self.copyfile(qemu, os.path.join(env['MOZ_UPLOAD_DIR'],
                                             os.path.basename(qemu)))

        tbpl_status, log_level = parser.evaluate_parser(return_code,
                                                        success_codes=success_codes)

        if os.path.isfile(logcat):
            if tbpl_status != TBPL_SUCCESS:
                # On failure, dump logcat, check if the emulator is still
                # running, and if it is still accessible via adb.
                self.info('dumping logcat')
                self.run_command(['cat', logcat], error_list=LogcatErrorList)

                self.run_command(['ps', '-C', 'emulator'])
                self.run_command([self.adb_path, 'devices'])

            # upload logcat to blobber
            self.copyfile(logcat, os.path.join(env['MOZ_UPLOAD_DIR'],
                                               os.path.basename(logcat)))
        else:
            self.info('no logcat file found')

        parser.append_tinderboxprint_line(suite_name)

        self.buildbot_status(tbpl_status, level=log_level)
        self.log("The %s suite: %s ran with return status: %s" %
                 (suite_name, suite, tbpl_status), level=log_level)

if __name__ == '__main__':
    emulatorTest = B2GEmulatorTest()
    emulatorTest.run_and_exit()
