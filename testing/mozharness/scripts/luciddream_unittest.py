#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import os
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import BaseErrorList, TarErrorList
from mozharness.base.log import ERROR, FATAL, INFO
from mozharness.base.script import (
    BaseScript,
    PreScriptAction,
)
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.buildbot import TBPL_SUCCESS, TBPL_WARNING, TBPL_FAILURE
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.unittest import TestSummaryOutputParserHelper
from mozharness.mozilla.gaia import GaiaMixin

class LuciddreamTest(TestingMixin, MercurialScript, MozbaseMixin, BaseScript,
                     BlobUploadMixin, TransferMixin, GaiaMixin):
    config_options = [[
        ["--emulator-url"],
        {"action": "store",
         "dest": "emulator_url",
         "default": None,
         "help": "URL to the emulator zip",
     }
    ], [
        ["--gaia-profile"],
        {"action": "store",
         "dest": "gaia_profile",
         "help": "path to gaia profile",
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
        ["--b2g-desktop-path"],
        {"action": "store",
         "dest": "b2gdesktop_path",
         "help": "path to b2gdesktop binary",
     }
    ], [
        ["--b2g-desktop-url"],
        {"action": "store",
         "dest": "b2gdesktop_url",
         "help": "URL to b2gdesktop archive",
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
    ]] + copy.deepcopy(testing_config_options) \
       + copy.deepcopy(blobupload_config_options)


    def __init__(self, require_config_file=False):
        super(LuciddreamTest, self).__init__(
            config_options=self.config_options,
            all_actions=['clobber',
                         'read-buildbot-config',
                         'download-and-extract',
                         'pull',
                         'create-virtualenv',
                         'install',
                         'run-tests'],
            default_actions=['clobber',
                             'read-buildbot-config',
                             'download-and-extract',
                             'pull',
                             'create-virtualenv',
                             'install',
                             'run-tests'],
            require_config_file=require_config_file,
            config={
                'require_test_zip': True,
                'emulator': 'arm',
                'virtualenv_modules': [
                    'mozinstall',
                ],
            }
        )

        # these are necessary since self.config is read only
        c = self.config
        self.installer_url = c.get('installer_url')
        self.installer_path = c.get('installer_path')
        self.emulator_url = c.get('emulator_url')
        self.binary_path = c.get('binary_path')
        self.test_url = c.get('test_url')
        self.test_packages_url = c.get('test_packages_url')

        if c.get('structured_output'):
            self.parser_class = StructuredOutputParser
        else:
            self.parser_class = TestSummaryOutputParserHelper

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(LuciddreamTest, self).query_abs_dirs()
        dirs = {}
        dirs['abs_blob_upload_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'logs')
        dirs['abs_test_install_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_emulator_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'emulator')
        dirs['abs_b2g-distro_dir'] = os.path.join(
            dirs['abs_emulator_dir'], 'b2g-distro')
        dirs['abs_b2g_desktop'] = os.path.join(
            abs_dirs['abs_work_dir'], 'b2gdesktop')
        gaia_root_dir = self.config.get('gaia_dir')
        if not gaia_root_dir:
            gaia_root_dir = self.config['base_work_dir']
        dirs['abs_gaia_dir'] = os.path.join(gaia_root_dir, 'gaia')
        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def install_emulator(self):
        dirs = self.query_abs_dirs()
        self.mkdir_p(dirs['abs_emulator_dir'])
        tarfile = self.download_file(self.emulator_url,
                                     parent_dir=dirs['abs_work_dir'],
                                     error_level=FATAL)
        self.emulator_path = tarfile
        tar = self.query_exe('tar', return_type='list')
        self.run_command(tar + ['zxf', self.emulator_path],
                         cwd=dirs['abs_emulator_dir'],
                         error_list=TarErrorList,
                         halt_on_failure=True, fatal_exit_code=3)

    def install(self, **kwargs):
        super(LuciddreamTest, self).install(**kwargs)
        dirs = self.query_abs_dirs()
        self.install_app(app='b2g',
                         target_dir=dirs['abs_b2g_desktop'],
                         installer_path=self.b2gdesktop_archive)

    def setup_gaia(self):
        dirs = self.query_abs_dirs()
        self.make_gaia(dirs['abs_gaia_dir'],
                       self.config.get('xre_path'),
                       xre_url=self.config.get('xre_url'),
                       debug=False,
                       noftu=False,
                       build_config_path=None)

    def pull(self, **kwargs):
        if self.config.get('b2gdesktop_url') or self.config.get('b2gdesktop_path'):
            GaiaMixin.pull(self, **kwargs)
        super(LuciddreamTest, self).pull(**kwargs)

    def download_and_extract(self):
        super(LuciddreamTest, self).download_and_extract()
        if self.config.get('emulator_url'):
            self.install_emulator()
        if self.config.get('b2gdesktop_url'):
            dirs = self.query_abs_dirs()
            self.mkdir_p(dirs['abs_b2g_desktop'])
            self.b2gdesktop_archive = self.download_file(self.config.get('b2gdesktop_url'),
                                                         parent_dir=dirs['abs_work_dir'])

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()
        requirements = os.path.join(dirs['abs_test_install_dir'],
                                    'config',
                                    'marionette_requirements.txt')
        self.register_virtualenv_module(requirements=[requirements],
                                        two_pass=True)

        luciddream_dir = os.path.join(dirs['abs_test_install_dir'],
                                      'luciddream')
        self.register_virtualenv_module(
            'luciddream',
            url=luciddream_dir,
            requirements=[os.path.join(luciddream_dir, 'requirements.txt')],
        )

    def run_tests(self):
        """
        Run the tests
        """
        dirs = self.query_abs_dirs()
        env = {}
        env = self.query_env(partial_env=env)

        ld_dir = os.path.join(dirs['abs_test_install_dir'], 'luciddream')

        if self.config.get('b2gdesktop_path') or self.config.get('b2gdesktop_url'):
            self.setup_gaia()

        ld_parser = self.parser_class(config=self.config,
                                      log_obj=self.log_obj,
                                      error_list=BaseErrorList)
 
        cmd = [self.query_python_path('python'),
               os.path.join(ld_dir, 'luciddream', 'runluciddream.py')
               ]

        str_format_values = {
            'browser_path': self.binary_path,
            'raw_log_file': os.path.join(dirs['abs_work_dir'], 'luciddream_raw.log'),
            'error_summary_file': os.path.join(dirs['abs_work_dir'], 'luciddream_errorsummary.log'),
            'test_manifest': os.path.join(ld_dir, 'example-tests', 'luciddream.ini')
        }

        if self.config.get('emulator_url'):
            str_format_values['emulator_path'] = dirs['abs_b2g-distro_dir']
        else:
            if self.config.get('b2gdesktop_url'):
                str_format_values['fxos_desktop_path'] = os.path.join(dirs['abs_b2g_desktop'], 'b2g', 'b2g')
            else:
                str_format_values['fxos_desktop_path'] = self.config.get('b2gdesktop_path')
            str_format_values['gaia_profile'] = os.path.join(dirs['abs_gaia_dir'], 'profile')

        suite = 'luciddream-emulator' if self.config.get('emulator_url') else 'luciddream-b2gdt'
        options = self.config['suite_definitions'][suite]['options']
        for option in options:
            option = option % str_format_values
            if not option.endswith('None'):
                cmd.append(option)

        code = self.run_command(cmd, env=env,
                                output_timeout=1000,
                                output_parser=ld_parser,
                                success_codes=[0],
                                cwd=dirs['abs_work_dir'])

        level = INFO
        if code == 0 and ld_parser.passed > 0 and ld_parser.failed == 0:
            status = "success"
            tbpl_status = TBPL_SUCCESS
        elif ld_parser.failed > 0:
            status = "test failures"
            tbpl_status = TBPL_WARNING
        else:
            status = "harness failures"
            level = ERROR
            tbpl_status = TBPL_FAILURE

        ld_parser.print_summary('luciddream')

        self.log("Luciddream exited with return code %s: %s" % (code, status),
                 level=level)
        self.buildbot_status(tbpl_status)

        if not os.path.exists(dirs['abs_blob_upload_dir']):
            self.mkdir_p(dirs['abs_blob_upload_dir'])
        for filename in os.listdir(dirs['abs_work_dir']):
            if filename.endswith('.log'):
                self.copyfile(os.path.join(dirs['abs_work_dir'], filename),
                              os.path.join(dirs['abs_blob_upload_dir'], filename))


if __name__ == '__main__':
    luciddreamTest = LuciddreamTest()
    luciddreamTest.run_and_exit()
