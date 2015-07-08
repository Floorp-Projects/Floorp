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

from mozharness.base.errors import TarErrorList, ZipErrorList
from mozharness.base.log import INFO, ERROR, WARNING, FATAL
from mozharness.base.script import PreScriptAction
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.buildbot import TBPL_SUCCESS, TBPL_WARNING, TBPL_FAILURE
from mozharness.mozilla.gaia import GaiaMixin
from mozharness.mozilla.testing.errors import LogcatErrorList
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.testing.unittest import TestSummaryOutputParserHelper
from mozharness.mozilla.structuredlog import StructuredOutputParser

class MarionetteTest(TestingMixin, MercurialScript, BlobUploadMixin, TransferMixin, GaiaMixin):
    config_options = [[
        ["--application"],
        {"action": "store",
         "dest": "application",
         "default": None,
         "help": "application name of binary"
         }
    ], [
        ["--app-arg"],
        {"action": "store",
         "dest": "app_arg",
         "default": None,
         "help": "Optional command-line argument to pass to the browser"
         }
    ], [
        ["--gaia-dir"],
        {"action": "store",
         "dest": "gaia_dir",
         "default": None,
         "help": "directory where gaia repo should be cloned"
         }
    ], [
        ["--gaia-repo"],
        {"action": "store",
         "dest": "gaia_repo",
         "default": "https://hg.mozilla.org/integration/gaia-central",
         "help": "url of gaia repo to clone"
         }
    ], [
        ["--gaia-branch"],
        {"action": "store",
         "dest": "gaia_branch",
         "default": "default",
         "help": "branch of gaia repo to clone"
         }
    ], [
        ["--test-type"],
        {"action": "store",
         "dest": "test_type",
         "default": "browser",
         "help": "The type of tests to run",
         }
    ], [
        ["--marionette-address"],
        {"action": "store",
         "dest": "marionette_address",
         "default": None,
         "help": "The host:port of the Marionette server running inside Gecko.  Unused for emulator testing",
         }
    ], [
        ["--emulator"],
        {"action": "store",
         "type": "choice",
         "choices": ['arm', 'x86'],
         "dest": "emulator",
         "default": None,
         "help": "Use an emulator for testing",
         }
    ], [
        ["--gaiatest"],
        {"action": "store_true",
         "dest": "gaiatest",
         "default": False,
         "help": "Runs gaia-ui-tests by pulling down the test repo and invoking "
                 "gaiatest's runtests.py rather than Marionette's."
         }
    ], [
        ["--test-manifest"],
        {"action": "store",
         "dest": "test_manifest",
         "default": "unit-tests.ini",
         "help": "Path to test manifest to run relative to the Marionette "
                 "tests directory",
         }
    ], [
        ["--xre-path"],
        {"action": "store",
         "dest": "xre_path",
         "default": "xulrunner-sdk",
         "help": "directory (relative to gaia repo) of xulrunner-sdk"
         }
    ], [
        ["--xre-url"],
        {"action": "store",
         "dest": "xre_url",
         "default": None,
         "help": "url of desktop xre archive"
         }
     ], [
           ["--gip-suite"],
           {"action": "store",
            "dest": "gip_suite",
            "default": None,
            "help": "gip suite to be executed. If no value is provided, manifest tbpl-manifest.ini will be used. the See Bug 1046694"
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
        ["--e10s"],
        {"action": "store_true",
         "dest": "e10s",
         "help": "Run tests with multiple processes. (Desktop builds only)",
        }
     ]] + copy.deepcopy(testing_config_options) \
        + copy.deepcopy(blobupload_config_options)

    error_list = [
        {'substr': 'FAILED (errors=', 'level': WARNING},
        {'substr': r'''Could not successfully complete transport of message to Gecko, socket closed''', 'level': ERROR},
        {'substr': r'''Connection to Marionette server is lost. Check gecko''', 'level': ERROR},
        {'substr': 'Timeout waiting for marionette on port', 'level': ERROR},
        {'regex': re.compile(r'''(TEST-UNEXPECTED|PROCESS-CRASH)'''), 'level': ERROR},
        {'regex': re.compile(r'''(\b((?!Marionette|TestMarionette|NoSuchElement|XPathLookup|NoSuchWindow|StaleElement|ScriptTimeout|ElementNotVisible|NoSuchFrame|InvalidResponse|Javascript|Timeout|InvalidElementState|NoAlertPresent|InvalidCookieDomain|UnableToSetCookie|InvalidSelector|MoveTargetOutOfBounds)\w*)Exception)'''), 'level': ERROR},
    ]

    repos = []

    def __init__(self, require_config_file=False):
        super(MarionetteTest, self).__init__(
            config_options=self.config_options,
            all_actions=['clobber',
                         'read-buildbot-config',
                         'pull',
                         'download-and-extract',
                         'create-virtualenv',
                         'install',
                         'run-marionette'],
            default_actions=['clobber',
                             'pull',
                             'download-and-extract',
                             'create-virtualenv',
                             'install',
                             'run-marionette'],
            require_config_file=require_config_file,
            config={'require_test_zip': True})

        # these are necessary since self.config is read only
        c = self.config
        self.installer_url = c.get('installer_url')
        self.installer_path = c.get('installer_path')
        self.binary_path = c.get('binary_path')
        self.test_url = c.get('test_url')
        self.test_packages_url = c.get('test_packages_url')

        if c.get('structured_output'):
            self.parser_class = StructuredOutputParser
        else:
            self.parser_class = TestSummaryOutputParserHelper

    def _pre_config_lock(self, rw_config):
        super(MarionetteTest, self)._pre_config_lock(rw_config)
        if not self.config.get('emulator') and not self.config.get('marionette_address'):
                self.fatal("You need to specify a --marionette-address for non-emulator tests! (Try --marionette-address localhost:2828 )")

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(MarionetteTest, self).query_abs_dirs()
        dirs = {}
        dirs['abs_test_install_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_marionette_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'marionette', 'marionette')
        dirs['abs_marionette_tests_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'marionette', 'tests', 'testing',
            'marionette', 'client', 'marionette', 'tests')
        dirs['abs_gecko_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'gecko')
        dirs['abs_emulator_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'emulator')
        dirs['abs_b2g-distro_dir'] = os.path.join(
            dirs['abs_emulator_dir'], 'b2g-distro')

        gaia_root_dir = self.config.get('gaia_dir')
        if not gaia_root_dir:
            gaia_root_dir = self.config['base_work_dir']
        dirs['abs_gaia_dir'] = os.path.join(gaia_root_dir, 'gaia')
        dirs['abs_gaiatest_dir'] = os.path.join(
            dirs['abs_gaia_dir'], 'tests', 'python', 'gaia-ui-tests')
        dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')

        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    @PreScriptAction('create-virtualenv')
    def _configure_marionette_virtualenv(self, action):
        # XXX Bug 981030 - hack to unbreak b2g18. Remove when b2g18 no longer supported
        try:
            branch = self.buildbot_config['properties']['branch']
        except:
            branch = None
        if self.tree_config.get('use_puppetagain_packages') or branch in ('mozilla-b2g18', 'mozilla-b2g18_v1_1_0_hd'):
            self.register_virtualenv_module('mozinstall')
            self.register_virtualenv_module(
                'marionette', os.path.join('tests', 'marionette'))

            return

        dirs = self.query_abs_dirs()
        requirements = os.path.join(dirs['abs_test_install_dir'],
                                    'config',
                                    'marionette_requirements.txt')
        if os.access(requirements, os.F_OK):
            self.register_virtualenv_module(requirements=[requirements],
                                            two_pass=True)
        else:
            # XXX Bug 879765: Dependent modules need to be listed before parent
            # modules, otherwise they will get installed from the pypi server.
            # XXX Bug 908356: This block can be removed as soon as the
            # in-tree requirements files propagate to all active trees.
            mozbase_dir = os.path.join('tests', 'mozbase')
            self.register_virtualenv_module(
                'manifestparser', os.path.join(mozbase_dir, 'manifestdestiny'))
            for m in ('mozfile', 'mozlog', 'mozinfo', 'moznetwork', 'mozhttpd',
                      'mozcrash', 'mozinstall', 'mozdevice', 'mozprofile',
                      'mozprocess', 'mozrunner'):
                self.register_virtualenv_module(
                    m, os.path.join(mozbase_dir, m))

            self.register_virtualenv_module(
                'marionette', os.path.join('tests', 'marionette'))

        if self.config.get('gaiatest'):
            requirements = os.path.join(self.query_abs_dirs()['abs_gaiatest_dir'],
                                        'tbpl_requirements.txt')
            self.register_virtualenv_module(
                'gaia-ui-tests',
                url=self.query_abs_dirs()['abs_gaiatest_dir'],
                method='pip',
                requirements=[requirements],
                editable=True)

    def pull(self, **kwargs):
        if self.config.get('gaiatest'):
            # clone the gaia dir
            dirs = self.query_abs_dirs()
            dest = dirs['abs_gaia_dir']

            repo = {
                'repo_path': self.config.get('gaia_repo'),
                'revision': 'default',
                'branch': self.config.get('gaia_branch')
            }

            if self.buildbot_config is not None:
                # get gaia commit via hgweb
                repo.update({
                    'revision': self.buildbot_config['properties']['revision'],
                    'repo_path': 'https://hg.mozilla.org/%s' % self.buildbot_config['properties']['repo_path']
                })

            self.clone_gaia(dest, repo,
                            use_gaia_json=self.buildbot_config is not None)

        super(MarionetteTest, self).pull(**kwargs)

    def _get_options_group(self, is_emulator, is_gaiatest):
        """
        Determine which in tree options group to use and return the
        appropriate key.
        """
        platform = 'emulator' if is_emulator else 'desktop'
        testsuite = 'gaiatest' if is_gaiatest else 'marionette'
        # Currently running marionette on an emulator means webapi
        # tests. This method will need to change if this does.
        if is_emulator and not is_gaiatest:
            testsuite = 'webapi'
        return '_'.join([testsuite, platform, 'options'])

    def download_and_extract(self):
        super(MarionetteTest, self).download_and_extract()

        if self.config.get('emulator'):
            dirs = self.query_abs_dirs()

            self.mkdir_p(dirs['abs_emulator_dir'])
            tar = self.query_exe('tar', return_type='list')
            self.run_command(tar + ['zxf', self.installer_path],
                             cwd=dirs['abs_emulator_dir'],
                             error_list=TarErrorList,
                             halt_on_failure=True, fatal_exit_code=3)

    def install(self):
        if self.config.get('emulator'):
            self.info("Emulator tests; skipping.")
        else:
            super(MarionetteTest, self).install()

    def preflight_run_marionette(self):
        """preflight commands for all tests"""
        # If the in tree config hasn't been loaded by a previous step, load it here.
        if not self.tree_config:
            self._read_tree_config()

    def run_marionette(self):
        """
        Run the Marionette tests
        """
        dirs = self.query_abs_dirs()

        raw_log_file = os.path.join(dirs['abs_blob_upload_dir'],
                                    'marionette_raw.log')
        config_fmt_args = {
            'type': self.config.get('test_type'),
            # emulator builds require a longer timeout
            'timeout': 60000 if self.config.get('emulator') else 10000,
            'profile': os.path.join(dirs['abs_gaia_dir'], 'profile'),
            'xml_output': os.path.join(dirs['abs_work_dir'], 'output.xml'),
            'html_output': os.path.join(dirs['abs_blob_upload_dir'], 'output.html'),
            'logcat_dir': dirs['abs_work_dir'],
            'emulator': 'x86' if os.path.isdir(os.path.join(dirs['abs_b2g-distro_dir'], 'out',
                'target', 'product', 'generic_x86')) else 'arm',
            'symbols_path': self.symbols_path,
            'homedir': os.path.join(dirs['abs_emulator_dir'], 'b2g-distro'),
            'binary': self.binary_path,
            'address': self.config.get('marionette_address'),
            'raw_log_file': raw_log_file,
            'gecko_log': dirs["abs_blob_upload_dir"],
            'this_chunk': self.config.get('this_chunk', 1),
            'total_chunks': self.config.get('total_chunks', 1)
        }

        self.info("The emulator type: %s" % config_fmt_args["emulator"])
        # build the marionette command arguments
        python = self.query_python_path('python')

        if self.config.get('gaiatest'):
            # make the gaia profile
            build_config = os.path.join(dirs['abs_gaia_dir'], 'tests',
                                        'python', 'gaia-ui-tests',
                                        'build_config.json')

            self.make_gaia(dirs['abs_gaia_dir'],
                           self.config.get('xre_path'),
                           xre_url=self.config.get('xre_url'),
                           debug=False,
                           noftu=False,
                           build_config_path=build_config)

            # write a testvars.json file
            testvars = os.path.join(dirs['abs_gaiatest_dir'],
                                    'gaiatest', 'testvars.json')
            with open(testvars, 'w') as f:
                f.write("""{"acknowledged_risks": true,
                            "skip_warning": true,
                            "settings": {
                              "time.timezone": "America/Los_Angeles",
                              "time.timezone.user-selected": "America/Los_Angeles"
                            }}
                        """)
            config_fmt_args['testvars'] = testvars

            # gaia-ui-tests on B2G desktop builds
            cmd = [python, '-u', os.path.join(dirs['abs_gaiatest_dir'],
                                              'gaiatest',
                                              'cli.py')]

            if not self.config.get('emulator'):
                # support desktop builds with and without a built-in profile
                binary_path = os.path.dirname(self.binary_path)
                if os.access(os.path.join(binary_path, 'b2g-bin'), os.F_OK):
                    # first, try to find and use b2g-bin
                    config_fmt_args['binary'] = os.path.join(binary_path, 'b2g-bin')
                else:
                    # if b2g-bin cannot be found we must use just b2g
                    config_fmt_args['binary'] = os.path.join(binary_path, 'b2g')

            # Bug 1046694
            # using a different manifest if a specific gip-suite is specified
            # --type parameter depends on gip-suite
            if self.config.get('gip_suite'):
                manifest = os.path.join(dirs['abs_gaiatest_dir'], 'gaiatest', 'tests', self.config.get('gip_suite'),
                                        'manifest.ini')
                if self.config.get('gip_suite') == "functional":
                    config_fmt_args['type'] = "b2g-external"
            else:
                # For <v2.2 branches using the old tbpl-manifest.ini and no split gip-suite
                manifest = os.path.join(dirs['abs_gaiatest_dir'], 'gaiatest', 'tests',
                    'tbpl-manifest.ini')

        else:
            # Marionette or Marionette-webapi tests
            cmd = [python, '-u', os.path.join(dirs['abs_marionette_dir'],
                                              'runtests.py')]

            manifest = os.path.join(dirs['abs_marionette_tests_dir'],
                                    self.config['test_manifest'])

            if self.config.get('app_arg'):
                config_fmt_args['app_arg'] = self.config['app_arg']

            if self.config.get('e10s'):
                cmd.append('--e10s')

            cmd.append('--gecko-log=%s' % os.path.join(dirs["abs_blob_upload_dir"],
                                                       'gecko.log'))

        options_group = self._get_options_group(self.config.get('emulator'),
                                                self.config.get('gaiatest'))

        if self.config.get("structured_output"):
            config_fmt_args["raw_log_file"]= "-"

        if options_group not in self.tree_config:
            # This allows using the new in-tree format
            options_group = options_group.split("_options")[0]
            if options_group not in self.tree_config["suite_definitions"]:
                self.fatal("%s is not defined in the in-tree config" %
                           options_group)
            for s in self.tree_config["suite_definitions"][options_group]["options"]:
                cmd.append(s % config_fmt_args)
        else:
            for s in self.tree_config[options_group]:
                cmd.append(s % config_fmt_args)

        if self.mkdir_p(dirs["abs_blob_upload_dir"]) == -1:
            # Make sure that the logging directory exists
            self.fatal("Could not create blobber upload directory")

        cmd.append(manifest)
        cmd = self.append_harness_extra_args(cmd)

        env = {}
        if self.query_minidump_stackwalk():
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path
        if self.config.get('gaiatest'):
            env['GAIATEST_ACKNOWLEDGED_RISKS'] = '1'
            env['GAIATEST_SKIP_WARNING'] = '1'
        env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
        env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']
        if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
            self.mkdir_p(env['MOZ_UPLOAD_DIR'])
        env = self.query_env(partial_env=env)

        marionette_parser = self.parser_class(config=self.config,
                                              log_obj=self.log_obj,
                                              error_list=self.error_list)
        code = self.run_command(cmd, env=env,
                                output_timeout=1000,
                                output_parser=marionette_parser)
        level = INFO
        if code == 0 and marionette_parser.passed > 0 and marionette_parser.failed == 0:
            status = "success"
            tbpl_status = TBPL_SUCCESS
        elif code == 10 and marionette_parser.failed > 0:
            status = "test failures"
            tbpl_status = TBPL_WARNING
        else:
            status = "harness failures"
            level = ERROR
            tbpl_status = TBPL_FAILURE

        # dump logcat output if there were failures
        if self.config.get('emulator'):
            if marionette_parser.failed != "0" or 'T-FAIL' in marionette_parser.tsummary:
                logcat = os.path.join(dirs['abs_work_dir'], 'emulator-5554.log')
                if os.access(logcat, os.F_OK):
                    self.info('dumping logcat')
                    self.run_command(['cat', logcat], error_list=LogcatErrorList)
                else:
                    self.info('no logcat file found')
        else:
            # .. or gecko.log if it exists
            gecko_log = os.path.join(self.config['base_work_dir'], 'gecko.log')
            if os.access(gecko_log, os.F_OK):
                self.info('dumping gecko.log')
                self.run_command(['cat', gecko_log])
                self.rmtree(gecko_log)
            else:
                self.info('gecko.log not found')

        marionette_parser.print_summary('marionette')

        self.log("Marionette exited with return code %s: %s" % (code, status),
                 level=level)
        self.buildbot_status(tbpl_status)


if __name__ == '__main__':
    marionetteTest = MarionetteTest()
    marionetteTest.run_and_exit()
